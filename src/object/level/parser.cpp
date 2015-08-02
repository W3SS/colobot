/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2014, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsiteс.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */

#include "object/level/parser.h"

#include "app/app.h"

#include "common/make_unique.h"

#include "common/resources/inputstream.h"
#include "common/resources/outputstream.h"
#include "common/resources/resourcemanager.h"

#include "object/robotmain.h"

#include "object/level/parserexceptions.h"

#include <string>
#include <exception>
#include <sstream>
#include <iomanip>
#include <set>

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

CLevelParser::CLevelParser()
{
    m_filename = "";
}

CLevelParser::CLevelParser(std::string filename)
{
    m_filename = filename;
}

CLevelParser::CLevelParser(std::string category, int chapter, int rank)
{
    m_filename = BuildScenePath(category, chapter, rank);
}

CLevelParser::CLevelParser(LevelCategory category, int chapter, int rank)
: CLevelParser(GetLevelCategoryDir(category), chapter, rank)
{}

std::string CLevelParser::BuildCategoryPath(std::string category)
{
    std::ostringstream outstream;
    outstream << "levels/";
    if (category == "perso" || category == "win" || category == "lost")
    {
        outstream << "other/";
    }
    else
    {
        outstream << category << "/";
    }
    return outstream.str();
}

std::string CLevelParser::BuildCategoryPath(LevelCategory category)
{
    return BuildCategoryPath(GetLevelCategoryDir(category));
}

std::string CLevelParser::BuildScenePath(std::string category, int chapter, int rank, bool sceneFile)
{
    std::ostringstream outstream;
    outstream << BuildCategoryPath(category);
    if (category == "custom")
    {
        outstream << CRobotMain::GetInstancePointer()->GetCustomLevelName(chapter);
        if (rank == 0)
        {
            if (sceneFile)
            {
                outstream << "/chaptertitle.txt";
            }
        }
        else
        {
            outstream << "/level" << std::setfill('0') << std::setw(3) << rank;
            if (sceneFile)
            {
                outstream << "/scene.txt";
            }
        }
    }
    else if (category == "perso")
    {
        assert(chapter == 0);
        assert(rank == 0);
        outstream << "perso.txt";
    }
    else if (category == "win" || category == "lost")
    {
        assert(chapter == 0);
        outstream << category << std::setfill('0') << std::setw(3) << rank << ".txt";
    }
    else
    {
        outstream << "chapter" << std::setfill('0') << std::setw(3) << chapter;
        if (rank == 000)
        {
            if (sceneFile)
            {
                outstream << "/chaptertitle.txt";
            }
        }
        else
        {
            outstream << "/level" << std::setfill('0') << std::setw(3) << rank;
            if (sceneFile)
            {
                outstream << "/scene.txt";
            }
        }
    }
    return outstream.str();
}

std::string CLevelParser::BuildScenePath(LevelCategory category, int chapter, int rank, bool sceneFile)
{
    return BuildScenePath(GetLevelCategoryDir(category), chapter, rank, sceneFile);
}

bool CLevelParser::Exists()
{
    return CResourceManager::Exists(m_filename);
}

void CLevelParser::Load()
{
    CInputStream file;
    file.open(m_filename);
    if (!file.is_open())
        throw CLevelParserException("Failed to open file: " + m_filename);

    char lang = CApplication::GetInstancePointer()->GetLanguageChar();

    std::string line;
    int lineNumber = 0;
    std::set<std::string> translatableLines;
    while (getline(file, line))
    {
        lineNumber++;

        boost::replace_all(line, "\t", " "); // replace tab by space

        // ignore comments
        std::size_t comment = line.find("//");
        if (comment != std::string::npos)
            line = line.substr(0, comment);

        boost::algorithm::trim(line);

        std::size_t pos = line.find_first_of(" \t\n");
        std::string command = line.substr(0, pos);
        if (pos != std::string::npos)
        {
            line = line.substr(pos + 1);
            boost::algorithm::trim(line);
        }
        else
        {
            line = "";
        }

        if (command.empty())
            continue;

        auto parserLine = MakeUnique<CLevelParserLine>(lineNumber, command);

        if (command.length() > 2 && command[command.length() - 2] == '.')
        {
            std::string baseCommand = command.substr(0, command.length() - 2);
            parserLine->SetCommand(baseCommand);

            char languageChar = command[command.length() - 1];
            if (languageChar == 'E' && translatableLines.count(baseCommand) == 0)
            {
                translatableLines.insert(baseCommand);
            }
            else if (languageChar == lang)
            {
                if (translatableLines.count(baseCommand) > 0)
                {
                    auto it = std::remove_if(
                        m_lines.begin(),
                        m_lines.end(),
                        [&baseCommand](const CLevelParserLineUPtr& line)
                        {
                            return line->GetCommand() == baseCommand;
                        });
                    m_lines.erase(it, m_lines.end());
                }

                translatableLines.insert(baseCommand);
            }
            else
            {
                continue;
            }
        }

        while (!line.empty())
        {
            pos = line.find_first_of("=");
            std::string paramName = line.substr(0, pos);
            boost::algorithm::trim(paramName);
            line = line.substr(pos + 1);
            boost::algorithm::trim(line);

            if (line[0] == '\"')
            {
                pos = line.find_first_of("\"", 1);
                if (pos == std::string::npos)
                    throw CLevelParserException("Unclosed \" in " + m_filename + ":" + boost::lexical_cast<std::string>(lineNumber));
            }
            else if (line[0] == '\'')
            {
                pos = line.find_first_of("'", 1);
                if (pos == std::string::npos)
                    throw CLevelParserException("Unclosed ' in " + m_filename + ":" + boost::lexical_cast<std::string>(lineNumber));
            }
            else
            {
                pos = line.find_first_of("=");
                if (pos != std::string::npos)
                {
                    std::size_t pos2 = line.find_last_of(" \t\n", line.find_last_not_of(" \t\n", pos-1));
                    if (pos2 != std::string::npos)
                        pos = pos2;
                }
                else
                {
                    pos = line.length()-1;
                }
            }
            std::string paramValue = line.substr(0, pos + 1);
            boost::algorithm::trim(paramValue);

            parserLine->AddParam(paramName, MakeUnique<CLevelParserParam>(paramName, paramValue));

            if (pos == std::string::npos)
                break;
            line = line.substr(pos + 1);
            boost::algorithm::trim(line);
        }

        AddLine(std::move(parserLine));
    }

    file.close();
}

void CLevelParser::Save()
{
    COutputStream file;
    file.open(m_filename);
    if (!file.is_open())
        throw CLevelParserException("Failed to open file: " + m_filename);

    for (auto& line : m_lines)
    {
        file << *(line.get()) << "\n";
    }

    file.close();
}

const std::string& CLevelParser::GetFilename()
{
    return m_filename;
}

void CLevelParser::AddLine(CLevelParserLineUPtr line)
{
    line->SetLevel(this);
    m_lines.push_back(std::move(line));
}

CLevelParserLine* CLevelParser::Get(std::string command)
{
    for (auto& line : m_lines)
    {
        if (line->GetCommand() == command)
            return line.get();
    }
    throw CLevelParserException("Command not found: " + command);
}
