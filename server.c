#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 8080

char username[64];       // נשמר להזדהות
char password[64];       // המקום שבו תתרחש הפריצה
int authenticated = 0;   // ישונה דרך overflow
int registered = 0;      // לצורך תצוגת הודעה לאחר הרשמה

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[4096] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 3);
    printf("Server running at http://localhost:%d\n", PORT);

    while (1) {
        authenticated = 0;
        registered = 0;
        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));

        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        read(client_fd, buffer, sizeof(buffer) - 1);
        printf("\n=== New Request ===\n%s\n", buffer);

        char *body = strstr(buffer, "\r\n\r\n");
        if (body) body += 4;

        if (body && strstr(buffer, "POST")) {
            // שליפת שדות מהבקשה
            char *user_field = strstr(body, "username=");
            if (user_field) {
                user_field += 9;
                sscanf(user_field, "%63[^&]", username);
            }

            char *pass_field = strstr(body, "password=");
            if (pass_field) {
                pass_field += 9;
                strcpy(password, pass_field);  // ⚠️ כאן מתבצע overflow
                char *amp = strchr(password, '&');  // חיתוך אם קיים action
                if (amp) *amp = '\0';
            }

            char *action_field = strstr(body, "action=");
            if (action_field && strstr(action_field, "Register")) {
                FILE *fp = fopen("users.txt", "a");
                if (fp) {
                    fprintf(fp, "%s %s\n", username, password);
                    fclose(fp);
                    registered = 1;
                    printf("[DEBUG] New user registered: %s\n", username);
                }
            } else if (!authenticated) {
                FILE *fp = fopen("users.txt", "r");
                if (fp) {
                    char file_user[64], file_pass[64];
                    while (fscanf(fp, "%s %s", file_user, file_pass) != EOF) {
                        if (strcmp(file_user, username) == 0 &&
                            strcmp(file_pass, password) == 0) {
                            authenticated = 1;
                            break;
                        }
                    }
                    fclose(fp);
                }
            }

            printf("[DEBUG] username: %s\n", username);
            printf("[DEBUG] password: %.20s...\n", password);
            printf("[DEBUG] authenticated: %d\n", authenticated);
        }

        const char *response;
        if (strstr(buffer, "POST")) {
            if (registered) {
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                           "<html><head><meta charset='UTF-8'></head><body style='text-align:center;padding-top:50px;'>"
                           "<h1 style='color:green;'>✅ Registration successful</h1>"
                           "<form method='GET' action='/'>"
                           "<button style='margin-top:20px;padding:10px 20px;'>Proceed to Login</button>"
                           "</form></body></html>";
            } else if (authenticated) {
                printf(">> Login successful (via overflow or valid credentials)\n");
                char *success_html = malloc(1024);
                snprintf(success_html, 1024,
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                         "<html><head><meta charset='UTF-8'>"
                         "<style>"
                         "body { font-family: Arial; background: #e0f7fa; padding: 40px; text-align: center; }"
                         ".btn { margin: 10px; padding: 12px 24px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; }"
                         ".btn:hover { background: #0056b3; }"
                         "</style></head><body>"
                         "<h1 style='color:green;'>✅ Access Granted</h1>"
                         "<p>Welcome to the secure area.</p>"
                         "<button class='btn'>Dashboard</button>"
                         "<button class='btn'>Help</button>"
                         "<button class='btn'>Logout</button>"
                         "</body></html>");
                response = success_html;
            } else {
                printf(">> Login failed\n");
                response = "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                           "<html><head><meta charset='UTF-8'></head><body style='text-align:center;padding-top:50px;'>"
                           "<h1 style='color:red;'>❌ Login failed</h1></body></html>";
            }
        } else {
            response =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                "<html><head><meta charset='UTF-8'>"
                "<style>"
                "body { font-family: Arial; background: #f2f2f2; padding: 40px; }"
                "form { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px gray; max-width: 350px; margin: auto; }"
                "input { display: block; margin: 10px 0; padding: 10px; width: 100%; border-radius: 5px; border: 1px solid #ccc; }"
                "input[type=submit] { background: #007bff; color: white; border: none; cursor: pointer; transition: 0.3s; }"
                "input[type=submit]:hover { background: #0056b3; }"
                "h2 { text-align: center; color: #333; }"
                "</style></head><body>"
                "<form method='POST'>"
                "<h2>Login or Register</h2>"
                "<input name='username' placeholder='Username'>"
                "<input name='password' type='password' placeholder='Password'>"
                "<input type='submit' name='action' value='Login'>"
                "<input type='submit' name='action' value='Register'>"
                "</form></body></html>";
        }

        write(client_fd, response, strlen(response));
        close(client_fd);
        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}
