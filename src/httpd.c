/* A TCP echo server.
 *
 * Receive a message on port 32000, turn it into upper case and return
 * it to the sender.
 *
 * Copyright (c) 2016, Marcel Kyas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Reykjavik University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MARCEL
 * KYAS NOR REYKJAVIK UNIVERSITY BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>

#define MESSAGE_SIZE 1024
#define RESPONSE_SIZE 2048

typedef struct {
	struct http_request {
		gchar * method;
		gchar * URL;
		gchar * version;
	} request;
	struct http_status {
		gchar * version;
		gchar * code;
		gchar * phrase;
	} status;
	struct http_header {
		gchar * name;
		gchar * value;
	} * headers;
	gchar * body;
} http_message;

void printMessage(http_message m);
http_message parseRequest(gchar * message);
gchar * generateResponse(http_message request, struct sockaddr_in client);

int main()
{
    int sockfd;
    struct sockaddr_in server, client;
    char message[MESSAGE_SIZE];

    // Create and bind a TCP socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Network functions need arguments in network byte order instead of
    // host byte order. The macros htonl, htons convert the values.
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(61284);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));
 
    // Before the server can accept messages, it has to listen to the
    // welcome port. A backlog of one connection is allowed.
    listen(sockfd, 1);

    for (;;) {
        // We first have to accept a TCP connection, connfd is a fresh
        // handle dedicated to this connection.
        socklen_t len = (socklen_t) sizeof(client);
        int connfd = accept(sockfd, (struct sockaddr *) &client, &len);

        // Receive from connfd, not sockfd.
        ssize_t n = recv(connfd, message, sizeof(message) - 1, 0);

        message[n] = '\0';
        fprintf(stdout, "Received:\n%s\n\n", message);

		http_message request = parseRequest(message);
		//printMessage(request);
		gchar * response = generateResponse(request, client);
		size_t size = strlen(response);
		send(connfd, response, size, 0);
		g_free(response);

        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}

http_message parseRequest(gchar * m) 
{
	gchar ** message = g_strsplit(m, "\r\n", -1);
	//Parse the request line
	gchar ** request_line = g_strsplit(message[0], " ", 3);
	struct http_request request = { 
		.method = g_strdup(request_line[0]), 
		.URL = g_strdup(request_line[1]), 
		.version = g_strdup(request_line[2]) 
	};
	g_strfreev(request_line);

	//Parse headers if exist
	struct http_header * headers = NULL;
	guint i;

	for(i = 1; strlen(message[i]) > 1; i++)
	{
		headers = (struct http_header *) realloc(headers, (i) * sizeof(struct http_header));
		gchar ** header_line = g_strsplit(message[i], " ", 2);
		struct http_header header = {
			.name = g_strdup(header_line[0]),
			.value = g_strdup(header_line[1])
		};
		g_strfreev(header_line);
		headers[i-1] = header;
	}

	//Add body if available, will otherwise be null
	gchar * body = g_strdup(message[i+1]);
	g_strfreev(message);

	return (http_message) { .request = request, .headers = headers, .body = body };
}

gchar * generateResponse(http_message m, struct sockaddr_in client)
{
	gchar * status = g_strdup_printf("%s 200 OK", m.request.version);
	gchar * body;
	if(g_strcmp0(m.request.method, "POST") == 0) 
	{
		body = g_strdup_printf("%s %s:%d\r\n%s", m.request.URL, inet_ntoa(client.sin_addr), (int) ntohs(client.sin_port), m.body);
	}
	else 
	{
		body = g_strdup_printf("%s %s:%d", m.request.URL, inet_ntoa(client.sin_addr), (int) ntohs(client.sin_port));
	}
	gchar * content = g_strdup_printf("<!DOCTYPE HTML>\n<html>\n<body>\n%s\n</body>\n</html>", body);
	gchar * headers = g_strdup_printf("Connection: close\r\nContent-Length: %d", (int) strlen(content));
	
	gchar * response = NULL;
	if(g_strcmp0(m.request.method, "GET") == 0)  response = g_strjoin("\r\n", status, headers, "", content, NULL);
	else if(g_strcmp0(m.request.method, "HEAD") == 0) response = g_strjoin("\r\n", status, headers, "", NULL);
	else if(g_strcmp0(m.request.method, "POST") == 0) response = g_strjoin("\r\n", status, headers, "", content, NULL);
	else response = g_strjoin(" ", m.request.version, "404", "NOT FOUND", NULL);
	
	g_free(status);
	g_free(content);
	g_free(headers);
	g_free(body);

	return response;
}

void printMessage(http_message m)
{
	g_fprintf(stdout, "HTTP message:\n");
	g_fprintf(stdout, "Request line: %s %s %s\r\n", m.request.method, m.request.URL, m.request.version);
	struct http_header * current_header = m.headers;
	while(current_header->name != NULL)
	{
		g_fprintf(stdout, "Header line: %s %s\r\n", current_header->name, current_header->value);
		current_header++;
	}
	
	g_fprintf(stdout, "Body content:\n%s\n", m.body);
	puts("---END OF MESSAGE---");
}

/*
http_message generateResponse(http_message m, struct sockaddr_in client)
{
	struct http_status status = {
		.version = m.request.version,
		.code = "200",
		.phrase = "OK"
	};
	
	gchar * content_length;
	struct http_header headers[3] = {
		(struct http_header) { .name = "Connection: ", .value = "close" },
		(struct http_header) { .name = "Content-Length: ", .value = content_length },
		(struct http_header) { .name = "Content-Type: ", .value = "text/html" }
	};
	
	gchar * body = g_strconcat("<!DOCTYPE html>\n<html>\n<body>\n", m.request.URL, " ", inet_ntoa(client.sin_addr), "\n</body>\n</html>", NULL;
	g_sprintf(content_length, "%d", strlen(inet_ntoa(client.sin_addr)));
	
	return (http_message) { .status = status, .headers = headers, .body = body};
}
*/