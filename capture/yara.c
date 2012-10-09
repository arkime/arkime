/* yara.c  -- Functions dealing with yara library
 *
 * Copyright 2012 AOL Inc. All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <string.h>
#include "glib.h"
#include "yara.h"
#include "moloch.h"

static YARA_CONTEXT *yContext = 0;

/******************************************************************************/
extern MolochConfig_t config;

/******************************************************************************/
void moloch_yara_report_error(const char* file_name, int line_number, const char* error_message)
{
    LOG("%s:%d: %s\n", file_name, line_number, error_message);
}
/******************************************************************************/
void moloch_yara_init()
{
    yr_init();
    yContext = yr_create_context();
    yContext->error_report_function = moloch_yara_report_error;

    if (config.yara) {
        FILE *rule_file;

        rule_file = fopen(config.yara, "r");

        if (rule_file != NULL) {
            yr_push_file_name(yContext, config.yara);
                                    
            int errors = yr_compile_file(rule_file, yContext);
            
            fclose(rule_file);
            
            if (errors) {
                exit (0);
            }
        } else {
            printf("yara could not open file: %s\n", config.yara);
            exit(1);
        }
    }
}

#ifdef DEBUG
/******************************************************************************/
void print_string(unsigned char* data, unsigned int length, int unicode)
{
    unsigned int i;
    char* str;
    
    str = (char*) (data);
    
    for (i = 0; i < length; i++)
    {
        if (str[i] >= 32 && str[i] <= 126)
        {
            printf("%c",str[i]);
        }
        else
        {
            printf("\\x%02x", str[i]);
        }
        
        if (unicode) i++;
    }

    printf("\n");
}
/******************************************************************************/
void print_hex_string(unsigned char* data, unsigned int length)
{
    unsigned int i;
    
    for (i = 0; i < length; i++)
    {
        printf("%02X ", data[i]);
    }

    printf("\n");
}

/******************************************************************************/
int callback(RULE* rule, void* data)
{
    TAG* tag;
    STRING* string;
    META* meta;
    MATCH* match;
    
    int rule_match;
    int string_found;
    int show = TRUE;
    int show_tags = TRUE;
    int show_meta = TRUE;
    int show_strings = TRUE;
        
    
    rule_match = (rule->flags & RULE_FLAGS_MATCH);

    
    if (show)
    {
        printf("%s ", rule->identifier);

        printf(" flags: %x ", rule->flags);
        
        if (show_tags)
        {           
            tag = rule->tag_list_head;
            
            printf("tag: [");
                        
            while(tag != NULL)
            {
                if (tag->next == NULL)
                {
                    printf("%s", tag->identifier);
                }
                else
                {
                    printf("%s,", tag->identifier);
                }
                                
                tag = tag->next;
            }   
            
            printf("] ");
        }
        
        if (show_meta)
        {
            meta = rule->meta_list_head;
            
            printf("meta: [");
           
            while(meta != NULL)
            {
                if (meta->type == META_TYPE_INTEGER)
                {
                    printf("%s=%lu", meta->identifier, meta->integer);
                }
                else if (meta->type == META_TYPE_BOOLEAN)
                {
                    printf("%s=%s", meta->identifier, (meta->boolean)?("true"):("false"));
                }
                else
                {
                    printf("%s=\"%s\"", meta->identifier, meta->string);
                }
            
                if (meta->next != NULL)
                    printf(",");
                                        
                meta = meta->next;
            }
        
            printf("] ");
        }
        
        /* show matched strings */
        
        if (show_strings)
        {
            string = rule->string_list_head;

            while (string != NULL)
            {
                string_found = string->flags & STRING_FLAGS_FOUND;
                
                if (string_found)
                {
                    match = string->matches_head;

                    while (match != NULL)
                    {
                        printf("0x%lx:%s: ", match->offset, string->identifier);
                        
                        if (IS_HEX(string))
                        {
                            print_hex_string(match->data, match->length);
                        }
                        else if (IS_WIDE(string))
                        {
                            print_string(match->data, match->length, TRUE);
                        }
                        else
                        {
                            print_string(match->data, match->length, FALSE);
                        }
                        
                        match = match->next;
                    }
                }

                string = string->next;
            }       
        }
    }
    printf("\n");
    
    return CALLBACK_CONTINUE;
}
#endif
/******************************************************************************/
int moloch_yara_callback(RULE* rule, MolochSession_t* session)
{
    char tagname[256];
    TAG* tag;

#ifdef DEBUG
    callback(rule, session);
#endif

    if (rule->flags & RULE_FLAGS_MATCH) {
        snprintf(tagname, sizeof(tagname), "yara:%s", rule->identifier); 
        moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tagname);
        tag = rule->tag_list_head;
        while(tag != NULL) {
            if (tag->identifier) {
                snprintf(tagname, sizeof(tagname), "yara:%s", tag->identifier); 
                moloch_nids_add_tag(session, MOLOCH_TAG_TAGS, tagname);
            }
            tag = tag->next;
        }
    }
    
    return CALLBACK_CONTINUE;
}
/******************************************************************************/
int yr_scan_mem_blocks(MEMORY_BLOCK* block, YARA_CONTEXT* context, YARACALLBACK callback, void* user_data);

void  moloch_yara_execute(MolochSession_t *session, char *data, int len, int first)
{
    MEMORY_BLOCK block;

    if (!config.yara)
        return;
    
    if (first) {
        block.data = (unsigned char *)data;
        block.size = len;
        block.base = 0;
    } else if (len == 1) {
        block.data = (unsigned char *)data+1;
        block.size = len-1;
        block.base = 1;
    } else {
        block.data = (unsigned char *)data+2;
        block.size = len-2;
        block.base = 2;
    }
    block.next = NULL;
    
    yr_scan_mem_blocks(&block, yContext, (YARACALLBACK)moloch_yara_callback, session);
    return;
}
/******************************************************************************/
void moloch_yara_exit()
{
    yr_destroy_context(yContext);
}
