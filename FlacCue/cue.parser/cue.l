%option noyywrap
%option reentrant
%option bison-bridge
%option header-file="cue.lex.h"
%option prefix="cue_"

%{
#include <string.h>
#include <assert.h>

#include "cue.h"
#include "cue.tab.h"

#define YY_DECL int cue_lex(CUE_STYPE * yylval_param, CUE_LTYPE *llocp, yyscan_t yyscanner)
#define YYSTYPE CUE_STYPE
#define YYLTYPE CUE_LTYPE

#define YY_EXTRA_TYPE struct CueParserExtra *

#define YY_INPUT(buffer, result, max_size) \
{ \
    int read = yyextra->readCallback(yyextra->context, buffer, max_size); \
    result = read == 0 ? YY_NULL : read; \
}

static char const* charAfterPrefix(char const* string, char const* prefix) {
    char const* prefixStart = strstr(string, prefix);
    if (prefixStart == NULL) {
        return NULL;
    } else {
        return prefixStart + strlen(prefix);
    }
}

static char const* readQuotedString(char const* openingQuote, char* dst) {
    assert(*openingQuote == '"');

    char const* inputChar = openingQuote + 1;

    while (*inputChar != '\"') {
        if (*inputChar == '\\') {
            inputChar++;
        }
        *(dst++) = *(inputChar++);
    }
    *(dst++) = '\0';
    inputChar++;

    return inputChar;
}

static char const* readUnquotedString(char const* firstChar, char* dst) {
    char const* inputChar = firstChar;

    while (*inputChar != ' ' && *inputChar != '\t' && *inputChar != '\r' && *inputChar != '\n' && *inputChar != '\0') {
        *(dst++) = *(inputChar++);
    }
    *(dst++) = '\0';

    return inputChar;
}

static char const* readString(char const* firstCharOrOpeningQuote, char* dst) {
    if (*firstCharOrOpeningQuote == '"') {
        return readQuotedString(firstCharOrOpeningQuote, dst);
    } else {
        return readUnquotedString(firstCharOrOpeningQuote, dst);
    }
}

static char const* readDoubleDigit(char const* firstChar, int* dst) {
    *dst = (int)(firstChar[0] - '0') * 10 + (int)(firstChar[1] - '0');
    return firstChar + 2;
}

static char const* readTime(char const* firstChar, struct CueTime* dst) {
    char const* inputChar = firstChar;
    inputChar = readDoubleDigit(inputChar, &dst->minutes);
    inputChar = readDoubleDigit(inputChar + 1, &dst->seconds);
    inputChar = readDoubleDigit(inputChar + 1, &dst->frames);
    return inputChar;
}

%}

DIGIT [0-9]
ALPHANUM [a-zA-Z0-9]
NEWLINE (\n|\r\n)
WHITESPACE [ \t\r\n]
NON_WHITESPACE [^ \t\r\n]
QUOTED_STRING \"(\\.|[^"])*\"
UNQUOTED_STRING {NON_WHITESPACE}*
STRING ({QUOTED_STRING}|{UNQUOTED_STRING})
INDENTATION ^{WHITESPACE}*
TIME ({DIGIT}{2}):({DIGIT}{2}):({DIGIT}{2})

%%
{INDENTATION}REM/{NEWLINE}	{
        yylval->command.rem = (char*)malloc(sizeof(char));
        yylval->command.rem[0] = '\0';
        return REM_COMMAND;
    }
{INDENTATION}REM\ [^\r\n]*/{NEWLINE}	{
        yylval->command.rem = (char*)malloc(sizeof(char) * yyleng);
        strcpy(yylval->command.rem, charAfterPrefix(yytext, "REM "));
        return REM_COMMAND;
    }
{INDENTATION}FLAGS\ [^\r\n]*/{NEWLINE}	{
        yylval->command.flags = (char*)malloc(sizeof(char) * yyleng);
        strcpy(yylval->command.rem, charAfterPrefix(yytext, "FLAGS "));
        return FLAGS_COMMAND;
    }
{INDENTATION}CATALOG\ ({DIGIT}{13})/{NEWLINE}	{
        yylval->command.catalog = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "CATALOG "), yylval->command.catalog);
        return CATALOG_COMMAND;
    }
{INDENTATION}PERFORMER\ {STRING}/{NEWLINE}	{
        yylval->command.performer = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "PERFORMER "), yylval->command.performer);
        return PERFORMER_COMMAND;
    }
{INDENTATION}TITLE\ {STRING}/{NEWLINE}	{
        yylval->command.title = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "TITLE "), yylval->command.title);
        return TITLE_COMMAND;
    }
{INDENTATION}CDTEXTFILE\ {STRING}/{NEWLINE}	{
        yylval->command.cdTextFile = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "CDTEXTFILE "), yylval->command.cdTextFile);
        return CDTEXTFILE_COMMAND;
    }
{INDENTATION}SONGWRITER\ {STRING}/{NEWLINE}	{
        yylval->command.songwriter = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "SONGWRITER "), yylval->command.songwriter);
        return SONGWRITER_COMMAND;
    }
{INDENTATION}ISRC\ ({ALPHANUM}{5})({DIGIT}{7})/{NEWLINE}	{
        yylval->command.isrc = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterPrefix(yytext, "ISRC "), yylval->command.isrc);
        return ISRC_COMMAND;
    }
{INDENTATION}FILE\ {STRING}\ {UNQUOTED_STRING}/{NEWLINE}	{
        yylval->command.file.path = (char*)malloc(sizeof(char) * yyleng);
        yylval->command.file.fileType = (char*)malloc(sizeof(char) * yyleng);
        char const* pathStart = charAfterPrefix(yytext, "FILE ");
        char const* charAfterPath = readString(pathStart, yylval->command.file.path);
        readString(charAfterPath + 1, yylval->command.file.fileType);
        return FILE_COMMAND;
    }
{INDENTATION}PREGAP\ {TIME}/{NEWLINE}	{
        readTime(charAfterPrefix(yytext, "PREGAP "), &yylval->command.preGap);
        return PREGAP_COMMAND;
    }
{INDENTATION}POSTGAP\ {TIME}/{NEWLINE}	{
        readTime(charAfterPrefix(yytext, "POSTGAP "), &yylval->command.postGap);
        return POSTGAP_COMMAND;
    }
{INDENTATION}INDEX\ ({DIGIT}{2})\ {TIME}/{NEWLINE}	{
        char const* charAfterIndex = readDoubleDigit(charAfterPrefix(yytext, "INDEX "), &yylval->command.index.index);
        readTime(charAfterIndex + 1, &yylval->command.index.time);
        return INDEX_COMMAND;
    }
{INDENTATION}TRACK\ ({DIGIT}{2})\ {UNQUOTED_STRING}/{NEWLINE}	{
        char const* charAfterTrackNumber = readDoubleDigit(charAfterPrefix(yytext, "TRACK "), &yylval->command.track.number);
        yylval->command.track.dataType = (char*)malloc(sizeof(char) * yyleng);
        readString(charAfterTrackNumber + 1, yylval->command.track.dataType);
        return TRACK_COMMAND;
    }
[ \t\r\n]
.	{ return INVALID_COMMAND; }
%%
