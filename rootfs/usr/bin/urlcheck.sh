#!/bin/sh

time=$(date +%H:%M:%S)

url=$1
if [ -z $url ]; then
    logger -s -p 3 -t "urlcheck" "[$time] url is null!!!"
    return
fi
logger -s -p 3 -t "urlcheck" "[$time] check url:$url"

#echo "http://www.baidu.com:80/ABCD/a.txt" | awk -F'[/:]' '{print $4}'

domain=$(echo $url | awk -F'[/:]' '{print $4}')
if [ -z $domain ]; then
    logger -s -p 3 -t "urlcheck" "[$time] domain is null!!!"
    return
fi
logger -s -p 3 -t "urlcheck" "[$time] check domain: $domain"

dns_res=$(nslookup $domain | grep Address)
logger -s -p 3 -t "urlcheck" "[$time] dns result: $dns_res"

curl_res=$(curl --insecure -s -IL $url | grep -E "HTTP/|Location:")
logger -s -p 3 -t "urlcheck" "[$time] curl result: $curl_res"

curl_res2=$(curl --insecure -o /dev/null -s -IL -w"http_code:%{http_code},localip:%{local_ip},remote_ip:%{remote_ip},num_redir:%{num_redirects},redirect_url:%{redirect_url},time_namelookup:%{time_namelookup},time_connect:%{time_connect},time_total:%{time_total} " $url)
logger -s -p 3 -t "urlcheck" "[$time] curl result: $curl_res2"

