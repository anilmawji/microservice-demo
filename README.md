# microservice-demo
Demonstration of client-server communication. The indirection server acts as a hub for connecting to various microservices (a translation server, a currency conversion server and a voting server). Made to work on a Linux environment.

Compile each file with the following commands:
`currency_server.c -o cur`
`voting_server.c -o vot`
`translate_server.c -o tra`
`indirection_server.c -o ind`
`main_client.c -o cli`

Run each file as follows:
`./cur`
`./vot`
`./tra`
`./ind`
`./cli 136.159.5.25 9043`
  
