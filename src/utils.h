#ifndef UTILS_H
#define UTILS_H

#include <string>

// Function to retrieve the OpenAI API key from the environment variables
bool get_openai_key(std::string& res);

// Function to retrieve the OpenAI base URL from the environment variables
bool get_openai_baseurl(std::string& res);

bool create_conversation_item(std::string& message, std::string& result);

bool create_response(std::string& result);

#endif // UTILS_H
