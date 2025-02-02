#ifndef UTILS_H
#define UTILS_H

#include <string>

// Function to retrieve the OpenAI API key from the environment variables
int get_openai_key(std::string& res);

// Function to retrieve the OpenAI base URL from the environment variables
int get_openai_baseurl(std::string& res);

#endif // UTILS_H
