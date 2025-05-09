//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//                        Main class of the FTP server
// 
//****************************************************************************

#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <list>
#include <iostream>
#include "common.h"
#include "FTPServer.h"
#include "ClientConnection.h"


int define_socket_TCP(int port = 0) {
   struct sockaddr_in sin;
   int s;

   s = socket(AF_INET, SOCK_STREAM, 0);

   if (s < 0) {
      errexit("Fallo en el socket: %s\n", strerror(errno));
   }

   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = INADDR_ANY;
   sin.sin_port = htons(port);

   if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
      close(s);
      errexit("Fallo en el bind: %s\n", strerror(errno));
   }

   if (listen(s, 5) < 0) {
      close(s);
      errexit("Fallo en el listen: %s\n", strerror(errno));
   }
   
   socklen_t size = sizeof(sin);
   if (getsockname(s, (struct sockaddr *) &sin, &size)) {
      close(s);
      errexit("No se puede leer el puerto del socket %s\n", strerror(errno));
   }

   std::cout << "Listening on port: " << ntohs(sin.sin_port) << std::endl;
   std::cout.flush();

   return s;
}

// This function is executed when the thread is executed.
void *run_client_connection(void *c) {
   ClientConnection *connection = (ClientConnection *) c;
   connection->WaitForRequests();
   return nullptr;
}

FTPServer::FTPServer(int port) {
   this->port = port;
}

// Stops the server
void FTPServer::stop() {
   close(msock);
   shutdown(msock, SHUT_RDWR);
}

// Starts of the server
void FTPServer::run() {
   struct sockaddr_in fsin;
   int ssock;
   socklen_t alen = sizeof(fsin);
   msock = define_socket_TCP(port); // This function must be implemented by you

   while (true) {
      pthread_t thread;
      ssock = accept(msock, (struct sockaddr *) &fsin, &alen);
      if (ssock < 0)
         errexit("Fallo en el accept: %s\n", strerror(errno));
      auto *connection = new ClientConnection(ssock);
      // Here a thread is created in order to process multiple requests simultaneously
      pthread_create(&thread, nullptr, run_client_connection, (void *) connection);
   }
}
