/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2010 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "executionpath.h"
#include "token.h"
#include <memory>
#include <set>
#include <iterator>
#include <iostream>


static void checkExecutionPaths_(const Token *tok, std::list<ExecutionPath *> &checks);


// default : bail out if the condition is has variable handling
bool ExecutionPath::parseCondition(const Token &tok, std::list<ExecutionPath *> & checks)
{
    unsigned int parlevel = 0;
    for (const Token *tok2 = &tok; tok2; tok2 = tok2->next())
    {
        if (tok2->str() == "(")
            ++parlevel;
        else if (tok2->str() == ")")
        {
            if (parlevel == 0)
                break;
            --parlevel;
        }
        else if (Token::Match(tok2, "[;{}]"))
            break;
        if (tok2->varId() != 0)
        {
            bailOutVar(checks, tok2->varId());
        }
    }

    for (std::list<ExecutionPath *>::iterator it = checks.begin(); it != checks.end();)
    {
        if ((*it)->varId > 0 && (*it)->numberOfIf >= 1)
        {
            delete *it;
            checks.erase(it++);
        }
        else
        {
            ++it;
        }
    }


    return false;
}


void ExecutionPath::print() const
{
    std::cout << " varId=" << varId
              << " numberOfIf=" << numberOfIf
              << "\n";
}

// I use this function when debugging ExecutionPaths with GDB
#ifdef __GNUC__
static void printchecks(const std::list<ExecutionPath *> &checks)
{
    for (std::list<ExecutionPath *>::const_iterator it = checks.begin(); it != checks.end(); ++it)
        (*it)->print();
}
#endif



/**
 * @brief Parse If/Switch body recursively.
 * @param tok First token in body.
 * @param checks The current checks
 * @param newchecks new checks
 * @param countif The countif set - count number of if for each execution path
 */
static void parseIfSwitchBody(const Token * const tok,
                              const std::list<ExecutionPath *> &checks,
                              std::list<ExecutionPath *> &newchecks,
                              std::set<unsigned int> &countif)
{
    std::set<unsigned int> countif2;
    std::list<ExecutionPath *> c;
    if (!checks.empty())
    {
        std::list<ExecutionPath *>::const_iterator it;
        for (it = checks.begin(); it != checks.end(); ++it)
        {
            if ((*it)->numberOfIf == 0)
                c.push_back((*it)->copy());
            if ((*it)->varId != 0)
                countif2.insert((*it)->varId);
        }
    }
    checkExecutionPaths_(tok, c);
    while (!c.empty())
    {
        if (c.back()->varId == 0)
        {
            c.pop_back();
            continue;
        }

        bool duplicate = false;
        std::list<ExecutionPath *>::const_iterator it;
        for (it = checks.begin(); it != checks.end(); ++it)
        {
            if (*(*it) == *c.back())
            {
                duplicate = true;
                countif2.erase((*it)->varId);
                break;
            }
        }
        if (!duplicate)
            newchecks.push_back(c.back());
        c.pop_back();
    }

    // Add countif2 ids to countif.. countif.
    countif.insert(countif2.begin(), countif2.end());
}


static void checkExecutionPaths_(const Token *tok, std::list<ExecutionPath *> &checks)
{
    if (!tok || tok->str() == "}" || checks.empty())
        return;

    const std::auto_ptr<ExecutionPath> check(checks.front()->copy());

    for (; tok; tok = tok->next())
    {
        if (tok->str() == "}" || tok->str() == "break")
            return;

        if (Token::simpleMatch(tok, "while ("))
        {
            // parse condition
            if (checks.size() > 10 || check->parseCondition(*tok->tokAt(2), checks))
            {
                ExecutionPath::bailOut(checks);
                return;
            }

            // skip "while (fgets()!=NULL)"
            if (Token::simpleMatch(tok, "while ( fgets ("))
            {
                const Token *tok2 = tok->tokAt(3)->link();
                if (Token::simpleMatch(tok2, ") ) {"))
                {
                    tok = tok2->tokAt(2)->link();
                    if (!tok)
                        break;
                    continue;
                }
            }
        }

        // goto => bailout
        if (tok->str() == "goto")
        {
            ExecutionPath::bailOut(checks);
            return;
        }

        // ?: => bailout
        if (tok->str() == "?")
        {
            ExecutionPath::bailOut(checks);
            return;
        }

        if (tok->str() == "switch")
        {
            // parse condition
            if (checks.size() > 10 || check->parseCondition(*tok->next(), checks))
            {
                ExecutionPath::bailOut(checks);
                return;
            }

            const Token *tok2 = tok->next()->link();
            if (Token::simpleMatch(tok2, ") { case"))
            {
                // what variable ids should the if be counted for?
                std::set<unsigned int> countif;

                std::list<ExecutionPath *> newchecks;

                for (tok2 = tok2->tokAt(2); tok2; tok2 = tok2->next())
                {
                    if (tok2->str() == "{")
                        tok2 = tok2->link();
                    else if (tok2->str() == "}")
                        break;
                    else if (tok2->str() == "case" &&
                             !Token::Match(tok2, "case %num% : ; case"))
                    {
                        parseIfSwitchBody(tok2, checks, newchecks, countif);
                    }
                }

                // Add newchecks to checks..
                std::copy(newchecks.begin(), newchecks.end(), std::back_inserter(checks));

                // Increase numberOfIf
                std::list<ExecutionPath *>::iterator it;
                for (it = checks.begin(); it != checks.end(); ++it)
                {
                    if (countif.find((*it)->varId) != countif.end())
                        (*it)->numberOfIf++;
                }
            }
        }

        // for/while/switch/do .. bail out
        if (Token::Match(tok, "for|while|switch|do"))
        {
            // goto {
            const Token *tok2 = tok->next();
            if (tok2 && tok2->str() == "(")
                tok2 = tok2->link();
            if (tok2 && tok2->str() == ")")
                tok2 = tok2->next();
            if (!tok2 || tok2->str() != "{")
            {
                ExecutionPath::bailOut(checks);
                return;
            }

            // skip { .. }
            tok2 = tok2->link();

            // if "do { .. } while ( .." , goto end of while..
            if (Token::simpleMatch(tok, "do {") && Token::simpleMatch(tok2, "} while ("))
                tok2 = tok2->tokAt(2)->link();

            // bail out all variables if the scope contains a "return"
            // bail out all variables used in this for/while/switch/do
            for (; tok && tok != tok2; tok = tok->next())
            {
                if (tok->str() == "return")
                    ExecutionPath::bailOut(checks);
                if (tok->varId())
                    ExecutionPath::bailOutVar(checks, tok->varId());
            }

            continue;
        }

        // bailout used variables in '; FOREACH ( .. ) { .. }'
        else if (tok->str() != "if" && Token::Match(tok->previous(), "[;{}] %var% ("))
        {
            // goto {
            const Token *tok2 = tok->next()->link();
            if (tok2 && tok2->str() == ")")
            {
                tok2 = tok2->next();
                if (tok2 && tok2->str() == "{")
                {
                    // goto "}"
                    tok2 = tok2->link();

                    // bail out all variables used in "{ .. }"
                    for (; tok && tok != tok2; tok = tok->next())
                    {
                        if (tok->varId())
                            ExecutionPath::bailOutVar(checks, tok->varId());
                    }
                }
            }
        }

        // .. ) { ... }  => bail out
        if (Token::simpleMatch(tok, ") {"))
        {
            ExecutionPath::bailOut(checks);
            return;
        }

        if (Token::Match(tok, "abort|exit ("))
        {
            ExecutionPath::bailOut(checks);
            return;
        }

        // might be a noreturn function..
        if (Token::Match(tok->previous(), "[;{}] %var% ( ) ; }") && tok->varId() == 0)
        {
            ExecutionPath::bailOut(checks);
            return;
        }

        // don't parse into "struct type { .."
        if (Token::Match(tok, "struct|union|class %type% {|:"))
        {
            while (tok && tok->str() != "{" && tok->str() != ";")
                tok = tok->next();
            tok = tok ? tok->link() : 0;
        }

        if (Token::Match(tok, "= {"))
        {
            // GCC struct initialization.. bail out
            if (Token::Match(tok->tokAt(2), ". %var% ="))
            {
                ExecutionPath::bailOut(checks);
                return;
            }

            tok = tok->next()->link();
            if (!tok)
            {
                ExecutionPath::bailOut(checks);
                return;
            }
            continue;
        }

        // ; { ... }
        if (Token::Match(tok->previous(), "[;{}] {"))
        {
            checkExecutionPaths_(tok->next(), checks);
            tok = tok->link();
            continue;
        }

        if (tok->str() == "if")
        {
            // what variable ids should the numberOfIf be counted for?
            std::set<unsigned int> countif;

            std::list<ExecutionPath *> newchecks;
            while (tok->str() == "if")
            {
                // goto "("
                tok = tok->next();

                // parse condition
                if (checks.size() > 10 || check->parseCondition(*tok->next(), checks))
                {
                    ExecutionPath::bailOut(checks);
                    ExecutionPath::bailOut(newchecks);
                    return;
                }

                // goto ")"
                tok = tok ? tok->link() : 0;

                // goto "{"
                tok = tok ? tok->next() : 0;

                if (!Token::simpleMatch(tok, "{"))
                {
                    ExecutionPath::bailOut(checks);
                    ExecutionPath::bailOut(newchecks);
                    return;
                }

                // Recursively check into the if ..
                parseIfSwitchBody(tok->next(), checks, newchecks, countif);

                // goto "}"
                tok = tok->link();

                // there is no else => break out
                if (Token::Match(tok, "} !!else"))
                    break;

                // parse next "if"..
                tok = tok->tokAt(2);
                if (tok->str() == "if")
                    continue;

                // there is no "if"..
                checkExecutionPaths_(tok->next(), checks);
                tok = tok->link();
                if (!tok)
                {
                    ExecutionPath::bailOut(newchecks);
                    return;
                }
            }

            // Add newchecks to checks..
            std::copy(newchecks.begin(), newchecks.end(), std::back_inserter(checks));

            // Increase numberOfIf
            std::list<ExecutionPath *>::iterator it;
            for (it = checks.begin(); it != checks.end(); ++it)
            {
                if (countif.find((*it)->varId) != countif.end())
                    (*it)->numberOfIf++;
            }
        }


        {
            tok = check->parse(*tok, checks);
            if (checks.empty())
                return;
        }

        // return/throw ends all execution paths
        if (tok->str() == "return" || tok->str() == "throw")
        {
            ExecutionPath::bailOut(checks);
        }
    }
}

void checkExecutionPaths(const Token *tok, ExecutionPath *c)
{
    for (; tok; tok = tok->next())
    {
        if (tok->str() != ")")
            continue;

        // Start of implementation..
        if (Token::Match(tok, ") const| {"))
        {
            // goto the "{"
            tok = tok->next();
            if (tok->str() == "const")
                tok = tok->next();

            std::list<ExecutionPath *> checks;
            checks.push_back(c->copy());
            checkExecutionPaths_(tok, checks);

            c->end(checks, tok->link());

            while (!checks.empty())
            {
                delete checks.back();
                checks.pop_back();
            }

            // skip this scope - it has been checked
            tok = tok->link();
        }
    }
}

