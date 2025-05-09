//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//                This class processes an FTP transaction.
// 
//****************************************************************************


#include <cstring>
#include <cstdio>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include "ClientConnection.h"

ClientConnection::ClientConnection(int s) {
   control_socket = s;
   // Check the Linux man pages to know what fdopen does.
   control_fd = fdopen(s, "a+");
   if (control_fd == nullptr) {
      std::cout << "Connection closed" << std::endl;
      fclose(control_fd);
      close(control_socket);
      ok = false;
      return;
   }
   ok = true;
   data_socket = -1;
   stop_server = false;
};

ClientConnection::~ClientConnection() {
   fclose(control_fd);
   close(control_socket);
}

int connect_TCP(uint32_t address, uint16_t port) {
   struct sockaddr_in sin;
   int s;

   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_port = htons(port);
   sin.sin_addr.s_addr = htonl(address);
   s = socket(AF_INET, SOCK_STREAM, 0);
   if (s < 0) {
      errexit("No se puede crear el socket: %s\n", strerror(errno));
   }

   if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      errexit("No se puede conectar: %s\n", strerror(errno));
   }

   return s;

}

void ClientConnection::stop() {
   close(data_socket);
   close(control_socket);
   stop_server = true;
}

#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.
void ClientConnection::WaitForRequests() {
   uint32_t ip = -1;
   uint16_t port = -1;
   if (!ok) {
      return;
   }
   fprintf(control_fd, "220 Service ready\n");
   while (!stop_server) {
      fscanf(control_fd, "%s", command);
      if (COMMAND("USER")) {

         fscanf(control_fd, "%s", arg);
         fprintf(control_fd, "331 User name ok, need password\n");

      } else if (COMMAND("PWD")) {
         if(getcwd(arg, sizeof(arg)))
            fprintf(control_fd, "257 \"%s\" is current directory\n", arg);
         else fprintf(control_fd, "550 could not get current directory\n");

      } else if (COMMAND("PASS")) {

         fscanf(control_fd, "%s", arg);
         if (strcmp(arg, "1234") == 0) {
            fprintf(control_fd, "230 User logged in\n");
         } else {
            fprintf(control_fd, "530 Not logged in.\n");
            stop_server = true;
            break;
         }

      } else if (COMMAND("PORT")) {

         unsigned h1, h2, h3, h4, p1, p2;

         if (fscanf(control_fd, "%u,%u,%u,%u,%u,%u", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
            fprintf(control_fd, "501 Syntax error in parameters or arguments.\n");
            continue;
         }

         ip = (h1 << 24) | (h2 << 16) | (h3 << 8) | h4;
         port = (p1 << 8) | p2;

         //conectamos el socket de datos
         data_socket = connect_TCP(ip, port);
         if (data_socket < 0) {
            fprintf(control_fd, "425 Can't open data connection.\n");
            continue;
         }
     
         fprintf(control_fd, "200 OK\n");

      } else if (COMMAND("PASV")) {

         int pasv_socket;
         struct sockaddr_in pasv_addr{};
         socklen_t addrlen = sizeof(pasv_addr);
      
         //crea socket pasivo
         pasv_socket = socket(AF_INET, SOCK_STREAM, 0);
         if (pasv_socket < 0) {
            fprintf(control_fd, "425 Can't open passive connection.\n");
            continue;
         }
      
         // asignar IP y puerto automático (puerto 0 = el SO elige uno libre)
         pasv_addr.sin_family = AF_INET;
         pasv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
         pasv_addr.sin_port = htons(0); 
      
         if (bind(pasv_socket, (struct sockaddr*)&pasv_addr, sizeof(pasv_addr)) < 0) {
            close(pasv_socket);
            fprintf(control_fd, "425 Can't bind passive socket.\n");
            continue;
         }
      
         if (listen(pasv_socket, 1) < 0) {
            close(pasv_socket);
            fprintf(control_fd, "425 Can't listen on passive socket.\n");
            continue;
         }
      
         // Obtener el puerto asignado realmente
         if (getsockname(pasv_socket, (struct sockaddr*)&pasv_addr, &addrlen) < 0) {
            close(pasv_socket);
            fprintf(control_fd, "425 Can't get socket name.\n");
            continue;
         }
      
         uint16_t port_num = ntohs(pasv_addr.sin_port);
      
         // Obtener IP local (usa 127.0.0.1 para localhost)
         // Si el servidor tiene IP pública, cámbialo dinámicamente
         std::string ip_str = "127.0.0.1"; 
         int h1, h2, h3, h4;
         sscanf(ip_str.c_str(), "%d.%d.%d.%d", &h1, &h2, &h3, &h4);
      
         int p1 = port_num / 256;
         int p2 = port_num % 256;
      
         fprintf(control_fd, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\n",
                 h1, h2, h3, h4, p1, p2);
         fflush(control_fd);
      
         // Aceptar conexión del cliente (bloqueante)
         struct sockaddr_in client_addr;
         socklen_t client_len = sizeof(client_addr);
         data_socket = accept(pasv_socket, (struct sockaddr*)&client_addr, &client_len);
         close(pasv_socket);
      
         if (data_socket < 0) {
            fprintf(control_fd, "425 Can't open data connection.\n");
            continue;
         }
         
      } else if (COMMAND("STOR")) {

         fscanf(control_fd, "%s", arg);

         FILE* file = fopen(arg, "wb");
         if (!file) {
            fprintf(control_fd, "550 Error opening file.\n");
            continue;
         }

         fprintf(control_fd, "150 File creation ok, about to open data connection\n");
         fflush(control_fd);

         //transferencia de datos
         char buffer[1024];
         ssize_t bytes;
         while ((bytes = recv(data_socket, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes, file);
         }

         fclose(file);
         close(data_socket);
         data_socket = -1;

         fprintf(control_fd, "226 Closing data connection\n");

      } else if (COMMAND("RETR")) {

         fscanf(control_fd, "%s", arg);
         FILE *file = fopen(arg, "rb");
         if (!file) {
            fprintf(control_fd, "550 File not found\n");
            continue;
         }

         fprintf(control_fd, "150 File status okay; about to open data connection\n");
         fflush(control_fd); 

         char buffer[1024];
         size_t bytes;
         while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(data_socket, buffer, bytes, 0);
         }

         fclose(file);
         close(data_socket);
         data_socket = -1;

         fprintf(control_fd, "226 Closing data connection\n");

      } else if (COMMAND("LIST")) {

         if (data_socket < 0) {
            fprintf(control_fd, "425 No data connection.\n");
            continue;
         }
      
         fprintf(control_fd, "125 List started OK.\n");
         fflush(control_fd);
      
         FILE *listing = popen("ls", "r");  //ejecuta ls
         if (!listing) {
            fprintf(control_fd, "450 Requested file action not taken.\n");
            continue;
         }
      
         char buffer[1024];
         while (fgets(buffer, sizeof(buffer), listing)) {
            send(data_socket, buffer, strlen(buffer), 0);  //envía línea por línea
         }
      
         pclose(listing);
         close(data_socket);
         data_socket = -1;
      
         fprintf(control_fd, "250 List completed successfully.\n");

      } else if (COMMAND("SYST")) {

         fprintf(control_fd, "215 UNIX Type: L8.\n");

      } else if (COMMAND("TYPE")) {

         fscanf(control_fd, "%s", arg);
         fprintf(control_fd, "200 OK\n");

      } else if (COMMAND("QUIT")) {

         fprintf(control_fd, "221 Service closing control connection. Logged out if appropriate.\n");
         close(data_socket);
         stop_server = true;
         break;

      } else {

         fprintf(control_fd, "502 Command not implemented.\n");
         fflush(control_fd);
         printf("Comando : %s %s\n", command, arg);
         printf("Error interno del servidor\n");

      }
   }

   fclose(control_fd);
};