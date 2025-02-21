#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include <string>
#include <map>
#include <memory>

class HttpAPI {
private:
    std::string url;
    std::string key;
    
    std::string performRequest(const std::string& url, const std::string& method, 
                             const std::map<std::string, std::string>& headers = {});
    std::map<std::string, std::string> getHeaders();

public:
    HttpAPI(const std::string& host, const std::string& key);
    ~HttpAPI(){};
    std::string getData(const std::map<std::string, std::string>& filters = {});
};

#endif // AUTHENTICATION_H
