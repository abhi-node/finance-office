/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <iostream>

#include "netoptions.hxx"

// the third parameter, bCmdFile, is unimplemented and hence unused
bool NetOptions::initOptions(int argc, char* argv[], bool)
{
    if (argc < 2)
    {
        std::cerr << prepareHelp();
        return false;
    }

    for (int i = 1; i < argc; i++)
    {
        OString argument = argv[i];

        if (argument == "-h"_ostr || argument == "--help"_ostr)
        {
            std::cout << prepareHelp();
            return false;
        }
        else if (argument == "-v"_ostr || argument == "--verbose"_ostr)
        {
            m_options["--verbose"_ostr] = ""_ostr;
        }
        else if (argument == "-n"_ostr || argument == "--dry-run"_ostr)
        {
            m_options["--dry-run"_ostr] = ""_ostr;
            // dry run implies verbose
            m_options["--verbose"_ostr] = "--dry-run"_ostr;
        }
        else if (argument == "-T"_ostr || argument == "--types"_ostr)
        {
            if (i + 1 < argc)
            {
                if (m_options.count("--types"_ostr) == 0)
                {
                    m_options["--types"_ostr] = argv[++i];
                }
                else
                {
                    m_options["--types"_ostr] += ";"_ostr + argv[++i];
                }
            }
            else
            {
                throw IllegalArgument("-T/--types must be followed by type name or wildcard"_ostr);
            }
        }
        else if (argument == "-X"_ostr || argument == "--extra-types"_ostr)
        {
            if (i + 1 < argc)
            {
                m_extra_input_files.emplace_back(argv[++i]);
            }
            else
            {
                throw IllegalArgument("-X/--extra-types must be followed by .rdb file"_ostr);
            }
        }
        else if (argument == "-O"_ostr || argument == "--output-dir"_ostr)
        {
            if (i + 1 < argc)
            {
                m_options["--output-dir"_ostr] = argv[++i];
            }
            else
            {
                throw IllegalArgument("-O/--output-dir must be followed by directory"_ostr);
            }
        }
        else
        {
            m_inputFiles.emplace_back(argument);
        }
    }

    if (m_inputFiles.empty())
    {
        throw IllegalArgument("at least one .rdb file must be provided"_ostr);
    }

    if (m_options.count("--output-dir"_ostr) == 0)
    {
        throw IllegalArgument("-O/--output-dir must be provided"_ostr);
    }

    return true;
}

OString NetOptions::prepareHelp()
{
    return prepareVersion() + R"(

About:
    netmaker is a tool for generating C# files from a type library generated by the UNOIDL compiler unoidl-write.
    The generated code files require a reference to the net_basetypes.dll assembly to build.

Usage:
    netmaker [-v|--verbose] [-n|--dry-run]
        [-T|--types <type name or wildcard>]
        [-X|--extra-types <.rdb file>]
        -O|--output-dir <output directory>
        <rdb file(s)>

Options:
    -h, --help
    Display this help message.

    -v, --verbose
    Log the name of every file created and type generated to stdout.

    -n, --dry-run
    Do not write generated files to disk. Implies --verbose.

    -T, --types <type name or wildcard>
    Specify a type name or a wildcard pattern to generate code for. This option can be specified multiple times. If not specified, all types in the given .rdb files are generated.

    -X, --extra-types <.rdb file>
    Use an .rdb file containing types to be taken into account without generating output for them. This option can be specified multiple times.

    -O, --output-dir <directory>
    Specify the directory to write generated files to.

Examples:
    netmaker --verbose -T com.acme.XSomething \
        -X types.rdb -O acme/ acmetypes.rdb

    netmaker --dry-run -T com.acme.* -X types.rdb \
        -X offapi.rdb -O acme/ acmetypes.rdb

    netmaker -X types.rdb -O acme/ \
        acmetypes.rdb moretypes.rdb
)"_ostr;
}

OString NetOptions::prepareVersion() const { return m_program + " version "_ostr + m_version; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
