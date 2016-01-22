//
//  cue.h
//  FlacCue
//
//  Created by Tamás Zahola on 04/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#ifndef cue_h
#define cue_h

enum CueCommandType {
    CueCommandTypeCatalog,
    CueCommandTypeCdTextFile,
    CueCommandTypeFile,
    CueCommandTypeFlags,
    CueCommandTypeIndex,
    CueCommandTypeIsrc,
    CueCommandTypePerformer,
    CueCommandTypePostgap,
    CueCommandTypePregap,
    CueCommandTypeRem,
    CueCommandTypeSongwriter,
    CueCommandTypeTitle,
    CueCommandTypeTrack
};

struct CueTime {
    int minutes;
    int seconds;
    int frames;
};

struct CueIndex {
    int index;
    struct CueTime time;
};

struct CueFile {
    char* path;
    char* fileType;
};

struct CueTrack {
    int number;
    char* dataType;
};

struct CueCommand {
    enum CueCommandType type;
    union {
        char* catalog;
        char* cdTextFile;
        struct CueFile file;
        char* flags;
        struct CueIndex index;
        char* isrc;
        char* performer;
        struct CueTime postGap;
        struct CueTime preGap;
        char* rem;
        char* songwriter;
        char* title;
        struct CueTrack track;
    };
};

struct CueCommandListItem {
    struct CueCommand command;
    struct CueCommandListItem* next;
};

struct CueCommandList {
    struct CueCommandListItem* head;
    struct CueCommandListItem* end;
};

void CueCommandListInit(struct CueCommandList* commandList);
void CueCommandListDestroy(struct CueCommandList* commandList);
void CueCommandListAppend(struct CueCommandList* commandList, struct CueCommand* command);
void CueCommandDestroy(struct CueCommand* command);

struct CueParserExtra {
    int (*readCallback)(void * context, void * buffer, int maxSize);
    void * context;
};

#endif /* cue_h */
