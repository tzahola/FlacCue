//
//  cue.c
//  FlacCue
//
//  Created by Tamás Zahola on 08/01/16.
//  Copyright © 2016 Tamás Zahola. All rights reserved.
//

#include "cue.h"

#include <stdlib.h>

void CueCommandListInit(struct CueCommandList* commandList) {
    commandList->head = NULL;
    commandList->end = NULL;
}

void CueCommandDestroy(struct CueCommand* command) {
    switch (command->type) {
        case CueCommandTypeCatalog: free(command->catalog); break;
        case CueCommandTypeCdTextFile: free(command->cdTextFile); break;
        case CueCommandTypeFile:
            free(command->file.fileType);
            free(command->file.path);
            break;
        case CueCommandTypeFlags: free(command->flags); break;
        case CueCommandTypeIsrc: free(command->isrc); break;
        case CueCommandTypePerformer: free(command->performer); break;
        case CueCommandTypeRem: free(command->rem); break;
        case CueCommandTypeSongwriter: free(command->songwriter); break;
        case CueCommandTypeTitle: free(command->title); break;
        case CueCommandTypeTrack: free(command->track.dataType); break;
        default: break;
    }
}

void CueCommandListDestroy(struct CueCommandList* commandList) {
    struct CueCommandListItem* item = commandList->head;
    while (item != NULL) {
        struct CueCommandListItem* next = item->next;
        CueCommandDestroy(&item->command);
        free(item);
        item = next;
    }
    commandList->head = NULL;
    commandList->end = NULL;
}

void CueCommandListAppend(struct CueCommandList* commandList, struct CueCommand* command) {
    struct CueCommandListItem* item = malloc(sizeof(*item));
    item->command = *command;
    item->next = NULL;
    
    if (commandList->head == NULL) {
        commandList->head = item;
    } else {
        commandList->end->next = item;
    }
    
    commandList->end = item;
}
