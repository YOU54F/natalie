require "socket"
require 'json'

# bin/natalie examples/http.rb /contracts/publish POST '{"pacticipantName":"foo","pacticipantVersionNumber":"foo","contracts":[{"consumerName":"foo","providerName":"foo","content":"e30K","contentType":"application/json","specification":"pact"}]}'
# bin/natalie examples/http.rb /metrics
#  echo '{"pacticipantName":"foo","pacticipantVersionNumber":"foo","contracts":[{"consumerName":"foo","providerName":"foo","content":"e30K","contentType":"application/json","specification":"pact"}]}' > pact.json
# bin/natalie examples/http.rb /contracts/publish POST pact.json

# No pretty print json method in natalie
def pretty_print_json(json_string, indent = 2)
        stack = []
        current_indent = 0
        pretty_json = ""
    
        json_string.each_char do |char|
            case char
            when '{', '['
                stack.push(char)
                pretty_json += char + "\n" + " " * (current_indent += indent)
            when '}', ']'
                stack.pop
                pretty_json += "\n" + " " * (current_indent -= indent) + char
            when ','
                pretty_json += char + "\n" + " " * current_indent
            else
                pretty_json += char
            end
        end
    
        pretty_json
end

# Get the URL and HTTP method from the command line arguments
url = ARGV[0] || "/"
http_method = ARGV[1] || "GET"
json_data = ARGV[2] || {}

# Create a new TCP socket and connect to the server
@socket = Socket.new(Socket::AF_INET, Socket::SOCK_STREAM)
@socket.connect(Socket.pack_sockaddr_in(9292, '0.0.0.0'))

# Send the HTTP request to the server
if http_method == "GET"
    request = "GET #{url} HTTP/1.0\r\nHost: 0.0.0.0\r\n\r\n"
elsif http_method == "POST"
    puts "Enter JSON data:"
    begin
        json_data = File.read(json_data)
    rescue Errno::ENOENT
        # File does not exist, use provided JSON data
    end
    request = "POST #{url} HTTP/1.0\r\nHost: 0.0.0.0\r\nContent-Type: application/json\r\nContent-Length: #{json_data.length}\r\n\r\n#{json_data}"
else
    puts "Invalid HTTP method"
    exit
end

@socket.write(request)

# Read the response from the server
response = @socket.read.split("\r\n\r\n").last

# Parse the JSON response and print the self href
if url == "/"
        parsed_response = JSON.parse(response)
        for links in parsed_response["_links"]
                links.each do |key,value|
                        if key && key[0] == "href"
                                puts "----"
                                puts "⭐ #{value[1]}"
                                puts ""
                                puts "🌐 #{key[1]}"
                        end
                end
        end
        puts "----"
elsif url == "/metrics"
        parsed_response = JSON.parse(response)
        printf("%-30s %s\n", "Key", "Value")
        printf("%-30s %s\n", "-" * 30, "-" * 55)
        parsed_response.each do |key, value|
        printf("%-50s %s\n", key.to_s.capitalize, value)
        end
else
        puts pretty_print_json(response)
end

# Close the socket
@socket.close