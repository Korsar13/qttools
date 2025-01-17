// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qdoccommandlineparser.h"

#include "utilities.h"

#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>

QDocCommandLineParser::QDocCommandLineParser()
    : QCommandLineParser(),
      defineOption(QStringList() << QStringLiteral("D")),
      dependsOption(QStringList() << QStringLiteral("depends")),
      highlightingOption(QStringList() << QStringLiteral("highlighting")),
      showInternalOption(QStringList() << QStringLiteral("showinternal")),
      redirectDocumentationToDevNullOption(QStringList()
                                           << QStringLiteral("redirect-documentation-to-dev-null")),
      noExamplesOption(QStringList() << QStringLiteral("no-examples")),
      indexDirOption(QStringList() << QStringLiteral("indexdir")),
      installDirOption(QStringList() << QStringLiteral("installdir")),
      outputDirOption(QStringList() << QStringLiteral("outputdir")),
      outputFormatOption(QStringList() << QStringLiteral("outputformat")),
      noLinkErrorsOption(QStringList() << QStringLiteral("no-link-errors")),
      autoLinkErrorsOption(QStringList() << QStringLiteral("autolink-errors")),
      debugOption(QStringList() << QStringLiteral("debug")),
      atomsDumpOption("atoms-dump"),
      prepareOption(QStringList() << QStringLiteral("prepare")),
      generateOption(QStringList() << QStringLiteral("generate")),
      logProgressOption(QStringList() << QStringLiteral("log-progress")),
      singleExecOption(QStringList() << QStringLiteral("single-exec")),
      includePathOption("I", "Add dir to the include path for header files.", "path"),
      includePathSystemOption("isystem", "Add dir to the system include path for header files.",
                              "path"),
      frameworkOption("F", "Add macOS framework to the include path for header files.",
                      "framework"),
      timestampsOption(QStringList() << QStringLiteral("timestamps")),
      useDocBookExtensions(QStringList() << QStringLiteral("docbook-extensions"))
{
    setApplicationDescription(QCoreApplication::translate("qdoc", "Qt documentation generator"));
    addHelpOption();
    addVersionOption();

    setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    addPositionalArgument("file1.qdocconf ...", QCoreApplication::translate("qdoc", "Input files"));

    defineOption.setDescription(QCoreApplication::translate(
            "qdoc", "Define the argument as a macro while parsing sources"));
    defineOption.setValueName(QStringLiteral("macro[=def]"));
    addOption(defineOption);

    dependsOption.setDescription(QCoreApplication::translate("qdoc", "Specify dependent modules"));
    dependsOption.setValueName(QStringLiteral("module"));
    addOption(dependsOption);

    highlightingOption.setDescription(QCoreApplication::translate(
            "qdoc", "Turn on syntax highlighting (makes qdoc run slower)"));
    addOption(highlightingOption);

    showInternalOption.setDescription(
            QCoreApplication::translate("qdoc", "Include content marked internal"));
    addOption(showInternalOption);

    redirectDocumentationToDevNullOption.setDescription(
            QCoreApplication::translate("qdoc",
                                        "Save all documentation content to /dev/null. Useful if "
                                        "someone is interested in qdoc errors only."));
    addOption(redirectDocumentationToDevNullOption);

    noExamplesOption.setDescription(
            QCoreApplication::translate("qdoc", "Do not generate documentation for examples"));
    addOption(noExamplesOption);

    indexDirOption.setDescription(QCoreApplication::translate(
            "qdoc", "Specify a directory where QDoc should search for index files to load"));
    indexDirOption.setValueName(QStringLiteral("dir"));
    addOption(indexDirOption);

    installDirOption.setDescription(QCoreApplication::translate(
            "qdoc",
            "Specify the directory where the output will be after running \"make install\""));
    installDirOption.setValueName(QStringLiteral("dir"));
    addOption(installDirOption);

    outputDirOption.setDescription(QCoreApplication::translate(
            "qdoc", "Specify output directory, overrides setting in qdocconf file"));
    outputDirOption.setValueName(QStringLiteral("dir"));
    addOption(outputDirOption);

    outputFormatOption.setDescription(QCoreApplication::translate(
            "qdoc", "Specify output format, overrides setting in qdocconf file"));
    outputFormatOption.setValueName(QStringLiteral("format"));
    addOption(outputFormatOption);

    noLinkErrorsOption.setDescription(
            QCoreApplication::translate("qdoc", "Do not print link errors (i.e. missing targets)"));
    addOption(noLinkErrorsOption);

    autoLinkErrorsOption.setDescription(
            QCoreApplication::translate("qdoc", "Show errors when automatic linking fails"));
    addOption(autoLinkErrorsOption);

    debugOption.setDescription(QCoreApplication::translate("qdoc", "Enable debug output"));
    addOption(debugOption);

    atomsDumpOption.setDescription(QCoreApplication::translate(
            "qdoc",
            "Shows a human-readable form of the intermediate result of parsing a block-comment."));
    addOption(atomsDumpOption);

    prepareOption.setDescription(QCoreApplication::translate(
            "qdoc", "Run qdoc only to generate an index file, not the docs"));
    addOption(prepareOption);

    generateOption.setDescription(QCoreApplication::translate(
            "qdoc", "Run qdoc to read the index files and generate the docs"));
    addOption(generateOption);

    logProgressOption.setDescription(
            QCoreApplication::translate("qdoc", "Log progress on stderr."));
    addOption(logProgressOption);

    singleExecOption.setDescription(
            QCoreApplication::translate("qdoc", "Run qdoc once over all the qdoc conf files."));
    addOption(singleExecOption);

    includePathOption.setFlags(QCommandLineOption::ShortOptionStyle);
    addOption(includePathOption);

    addOption(includePathSystemOption);

    frameworkOption.setFlags(QCommandLineOption::ShortOptionStyle);
    addOption(frameworkOption);

    timestampsOption.setDescription(
            QCoreApplication::translate("qdoc", "Timestamp each qdoc log line."));
    addOption(timestampsOption);

    useDocBookExtensions.setDescription(QCoreApplication::translate(
            "qdoc", "Use the DocBook Library extensions for metadata."));
    addOption(useDocBookExtensions);
}

/*!
 * \internal
 *
 * Create a list of arguments from the command line and/or file(s).
 * This lets QDoc accept arguments contained in a file provided as a
 * command-line argument prepended by '@'.
 */
static QStringList argumentsFromCommandLineAndFile(const QStringList &arguments)
{
    QStringList allArguments;
    allArguments.reserve(arguments.size());
    for (const QString &argument : arguments) {
        // "@file" doesn't start with a '-' so we can't use QCommandLineParser for it
        if (argument.startsWith(QLatin1Char('@'))) {
            QString optionsFile = argument;
            optionsFile.remove(0, 1);
            if (optionsFile.isEmpty())
                qFatal("The @ option requires an input file");
            QFile f(optionsFile);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                qFatal("Cannot open options file specified with @: %ls",
                       qUtf16Printable(optionsFile));
            while (!f.atEnd()) {
                QString line = QString::fromLocal8Bit(f.readLine().trimmed());
                if (!line.isEmpty())
                    allArguments << line;
            }
        } else {
            allArguments << argument;
        }
    }
    return allArguments;
}

void QDocCommandLineParser::process(const QStringList &arguments)
{
    auto allArguments = argumentsFromCommandLineAndFile(arguments);
    QCommandLineParser::process(allArguments);

    if (isSet(singleExecOption) && isSet(indexDirOption))
        qDebug("WARNING: -indexdir option ignored: Index files are not used in single-exec mode.");
}
