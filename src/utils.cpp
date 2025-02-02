#include <string>

int get_openai_key(std::string& res) {

    const char* api_key = getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        fprintf(stderr, "[Error]: OPENAI_API_KEY not set or empty in environment.\n");
        return -1;
    }
    res.assign(api_key);
    return 0;
}

int get_openai_baseurl(std::string& res) {
    const char* base_url = getenv("OPENAI_REALTIMEAPI");
    if (!base_url || std::string(base_url).empty()) {
        fprintf(stderr, "[Error]: OPENAI_REALTIMEAPI not set or empty in environment.\n");
        return -1;
    }
    res.assign(base_url);
    return 0;
}
