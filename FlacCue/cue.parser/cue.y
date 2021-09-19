%{
#include <stdio.h>
#include <string.h>
#include "cue.h"
#include "cue.tab.h"

#define YY_DECL int cue_lex(CUE_STYPE* yylval_param, CUE_LTYPE* llocp, yyscan_t yyscanner)
#define YYSTYPE CUE_STYPE
#define YYLTYPE CUE_LTYPE

#include "cue.lex.h"
extern YY_DECL;

static void yyerror(YYLTYPE* llocp, void* scanner, struct CueCommandList* result, char** error, int* errorLine, const char* errorMessage) {
    if (error != NULL) {
        *error = strdup(errorMessage);
        *errorLine = llocp->last_line;
    }
}

%}

%define api.pure full
%define api.prefix {cue_}
%define parse.error verbose
%lex-param { void* scanner }
%locations
%parse-param { void* scanner } { struct CueCommandList* result } { char** error } { int* errorLine }

%union {
    struct CueCommand command;
    char* invalidCommand;
}

%token<invalidCommand> INVALID_COMMAND
%destructor { free($$); } INVALID_COMMAND

%token<command> CATALOG_COMMAND
                CDTEXTFILE_COMMAND
                FLAGS_COMMAND
                PERFORMER_COMMAND
                ISRC_COMMAND
                SONGWRITER_COMMAND
                TITLE_COMMAND
                REM_COMMAND
                FILE_COMMAND
                INDEX_COMMAND
                POSTGAP_COMMAND
                PREGAP_COMMAND
                TRACK_COMMAND
%type<command> command

%%

commandList
    : {}
    | commandList command {
        CueCommandListAppend(result, &$2);
    }
;

command
    : CATALOG_COMMAND {
        $1.type = CueCommandTypeCatalog;
        $$ = $1;
    }
    | CDTEXTFILE_COMMAND {
        $1.type = CueCommandTypeCdTextFile;
        $$ = $1;
    }
    | FLAGS_COMMAND {
        $1.type = CueCommandTypeFlags;
        $$ = $1;
    }
    | PERFORMER_COMMAND {
        $1.type = CueCommandTypePerformer;
        $$ = $1;
    }
    | ISRC_COMMAND {
        $1.type = CueCommandTypeIsrc;
        $$ = $1;
    }
    | SONGWRITER_COMMAND {
        $1.type = CueCommandTypeSongwriter;
        $$ = $1;
    }
    | TITLE_COMMAND {
        $1.type = CueCommandTypeTitle;
        $$ = $1;
    }
    | FILE_COMMAND {
        $1.type = CueCommandTypeFile;
        $$ = $1;
    }
    | INDEX_COMMAND {
        $1.type = CueCommandTypeIndex;
        $$ = $1;
    }
    | POSTGAP_COMMAND {
        $1.type = CueCommandTypePostgap;
        $$ = $1;
    }
    | PREGAP_COMMAND {
        $1.type = CueCommandTypePregap;
        $$ = $1;
    }
    | TRACK_COMMAND {
        $1.type = CueCommandTypeTrack;
        $$ = $1;
    }
    | REM_COMMAND {
        $1.type = CueCommandTypeRem;
        $$ = $1;
    }
;

%%
