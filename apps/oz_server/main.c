#include "oz/oz_core.h"
#include "oz/oz_log.h"
#include "oz/render/oz_bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int listen_on(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 16) < 0) { close(fd); return -1; }
    return fd;
}

static void write_all(int fd, const char* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
}

static char* map_to_json(const OzMap* map) {
    // conservative estimate: ~128 chars per brush
    size_t cap = 128 + map->count * 128;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len, "{\"ok\":true,\"brushes\":[");
    for (size_t i = 0; i < map->count; ++i) {
        const OzBrush* br = &map->brushes[i];
        if (br->type == OZ_BRUSH_BOX) {
            const OzBrushBox* b = &br->as.box;
            len += (size_t)snprintf(out + len, cap - len,
                "%s{\"type\":\"box\",\"center\":[%.3f,%.3f,%.3f],\"size\":[%.3f,%.3f,%.3f]}",
                (i ? "," : ""),
                b->center.x, b->center.y, b->center.z,
                b->half.x * 2.0f, b->half.y * 2.0f, b->half.z * 2.0f);
        } else if (br->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* c = &br->as.cyl;
            len += (size_t)snprintf(out + len, cap - len,
                "%s{\"type\":\"cyl\",\"center\":[%.3f,%.3f,%.3f],\"rx\":%.3f,\"ry\":%.3f,\"h\":%.3f,\"segments\":%d}",
                (i ? "," : ""),
                c->center.x, c->center.y, c->center.z, c->radius_x, c->radius_y, c->height, c->segments);
        }
        if (len + 128 > cap) {
            cap *= 2;
            char* tmp = (char*)realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
    }
    len += (size_t)snprintf(out + len, cap - len, "]}");
    return out;
}

static void http_respond_json(int cfd, const char* body) {
    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        strlen(body));
    write_all(cfd, hdr, (size_t)hl);
    write_all(cfd, body, strlen(body));
}

static void http_respond_404(int cfd) {
    const char* body = "{\"ok\":false,\"error\":\"not found\"}";
    char hdr[256];
    int hl = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        strlen(body));
    write_all(cfd, hdr, (size_t)hl);
    write_all(cfd, body, strlen(body));
}

static const char* get_query_param(const char* path, const char* key, char* out_value, size_t out_cap) {
    const char* q = strchr(path, '?');
    if (!q) return NULL;
    ++q;
    size_t keylen = strlen(key);
    while (*q) {
        if (strncmp(q, key, keylen) == 0 && q[keylen] == '=') {
            q += keylen + 1;
            size_t i = 0;
            while (*q && *q != '&' && i + 1 < out_cap) out_value[i++] = *q++;
            out_value[i] = '\0';
            return out_value;
        }
        while (*q && *q != '&') ++q;
        if (*q == '&') ++q;
    }
    return NULL;
}

static void handle_client(int cfd) {
    char req[4096];
    ssize_t n = read(cfd, req, sizeof(req)-1);
    if (n < 0) n = 0; req[n] = '\0';
    // Parse method and path
    char method[8] = {0};
    char path[1024] = {0};
    sscanf(req, "%7s %1023s", method, path);

    if (strcmp(method, "GET") == 0 && strncmp(path, "/map", 4) == 0) {
        char name[256] = {0};
        if (!get_query_param(path, "name", name, sizeof(name))) {
            strcpy(name, "sample.ozone");
        }
        OzMap map; oz_map_init(&map);
        if (!oz_map_load_text(name, &map)) {
            OZ_WARN("Failed to load map: %s", name);
            http_respond_404(cfd);
            return;
        }
        char* json = map_to_json(&map);
        oz_map_free(&map);
        if (!json) { http_respond_404(cfd); return; }
        http_respond_json(cfd, json);
        free(json);
        return;
    }

    if (strcmp(method, "POST") == 0 && strncmp(path, "/auth/login", 11) == 0) {
        const char* body = "{\"ok\":true,\"token\":\"devtoken\"}";
        http_respond_json(cfd, body);
        return;
    }

    http_respond_404(cfd);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    OZ_INFO("OzWorld server %s starting", oz_core_version());
    int lfd = listen_on(8080);
    if (lfd < 0) {
        OZ_ERROR("listen failed: %s", strerror(errno));
        return 1;
    }
    OZ_INFO("Listening on http://127.0.0.1:8080");
    while (1) {
        struct sockaddr_in caddr; socklen_t clen = sizeof(caddr);
        int cfd = accept(lfd, (struct sockaddr*)&caddr, &clen);
        if (cfd < 0) continue;
        handle_client(cfd);
        close(cfd);
    }
    close(lfd);
    return 0;
}
