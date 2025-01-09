#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

// Define a structure to hold the parsed data
typedef struct {
    char type[50];
    char event_id[50];
    struct {
        char id[50];
        char object[50];
        char model[100];
        char expires_at[50];
        char modalities[10][50];
        int modality_count;
        char instructions[500];
        char voice[50];
    } session;
} ParsedData;

void parse_json(const char *json_string, ParsedData *data) {
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        printf("Error parsing JSON\n");
        return;
    }

    // Parse "type"
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (type && cJSON_IsString(type)) {
        snprintf(data->type, sizeof(data->type), "%s", type->valuestring);
    }

    // Parse "event_id"
    cJSON *event_id = cJSON_GetObjectItem(json, "event_id");
    if (event_id && cJSON_IsString(event_id)) {
        snprintf(data->event_id, sizeof(data->event_id), "%s", event_id->valuestring);
    }

    // Parse "session"
    cJSON *session = cJSON_GetObjectItem(json, "session");
    if (session && cJSON_IsObject(session)) {
        cJSON *id = cJSON_GetObjectItem(session, "id");
        if (id && cJSON_IsString(id)) {
            snprintf(data->session.id, sizeof(data->session.id), "%s", id->valuestring);
        }

        cJSON *object = cJSON_GetObjectItem(session, "object");
        if (object && cJSON_IsString(object)) {
            snprintf(data->session.object, sizeof(data->session.object), "%s", object->valuestring);
        }

        cJSON *model = cJSON_GetObjectItem(session, "model");
        if (model && cJSON_IsString(model)) {
            snprintf(data->session.model, sizeof(data->session.model), "%s", model->valuestring);
        }

        cJSON *expires_at = cJSON_GetObjectItem(session, "expires_at");
        if (expires_at && cJSON_IsString(expires_at)) {
            snprintf(data->session.expires_at, sizeof(data->session.expires_at), "%s", expires_at->valuestring);
        }

        cJSON *modalities = cJSON_GetObjectItem(session, "modalities");
        if (modalities && cJSON_IsArray(modalities)) {
            int count = 0;
            cJSON *modality;
            cJSON_ArrayForEach(modality, modalities) {
                if (cJSON_IsString(modality)) {
                    snprintf(data->session.modalities[count], sizeof(data->session.modalities[count]), "%s", modality->valuestring);
                    count++;
                }
            }
            data->session.modality_count = count;
        }

        cJSON *instructions = cJSON_GetObjectItem(session, "instructions");
        if (instructions && cJSON_IsString(instructions)) {
            snprintf(data->session.instructions, sizeof(data->session.instructions), "%s", instructions->valuestring);
        }

        cJSON *voice = cJSON_GetObjectItem(session, "voice");
        if (voice && cJSON_IsString(voice)) {
            snprintf(data->session.voice, sizeof(data->session.voice), "%s", voice->valuestring);
        }
    }

    // Clean up cJSON object
    cJSON_Delete(json);
}

