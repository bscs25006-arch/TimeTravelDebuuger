# Progress Log

 2 October 2026
 Set up Ubuntu to compile and run on Linux.
 Implemented stack and timeline
Implemented Pass 0x0 validation and tested it
 Started Pass 0x1 resolve bin
3 october 2026
again complete the resolve program 
and fixing bugs in my code that consume time 
and test main 
4 october 
creating helper functions so the execute program does nt have to run again and again
7 october :
still doing the resolve bin cuz ive issues regarding it and most things i dont really pick fast(concepts)

9 october:
adding the timeline and understanding of execute program and fixes issues in resolve program
10 october:
execute program
11 october:
write tbdg helper functions 
13 october:
completed check on ubuntu 

6 november:
complete the queue and developed teh understanding of client and server
7 november :

LOADER COMPLETE IMPLEMENTAION:
loader mei meinay pehlay queue complete ki or  j o function tha sendall usme implenetation yeh ki thi k agr sari bytes transfer hojyain toh or check up add kernay hain 
or sendError mei agr bytes transfer na hun toh error ir error ka bhi like w ehave to implement the error conditions also like specific headings for errors
so we have error message instaed of nothing  or ohir humnay different types k socket banaay hain like listen socket jisme humanay AFINET (ADDRESS FAMILY) INET (IPV4) OR SOCK_STREAM matlab TCP
OR SOCK_CLOEXEC ek extra flag hai. Iska matlab:agar yeh program koi aur program shuru kare (jaise system("./server &")) to naye program mein yeh socket apne aap band ho jaye is se server ko load balancer ke sockets ki copy nahi milti.
or phir connect to socket mei same naya socket 
or phir connection k liyee start server and then attempts tak server ko connect kero 
relayononeclient mei humanay sab se phela serever wake up krna hai then hum ne client se bytes ko thora thora transfer kerna hais server tk or phir server se recieve ker k client tak
or phir c.sock or serversock band kerni hai 
or acceptclients mei hum clients ko accept kertay hain max 8 
or dispatcherloop mei sirf oneclient se deal kertay hain


OH HAN LAST PHASE1 MEI IDK K HAR FUNCTION KO EXPLAIN KERNA THA 

8 NOVEMBER:
or manay client ko smjha hai or sendall  and receive all k functions banaye hain helper mei cuz we can use from helper in client to receive all the bytes count the total bytes and loop will run until n bytes or sendall bhi bss usme MSG_NOTIONAL USE kia hai jo k agar samne wala pehle hi disconnect ho chuka ho Linux normally program ko SIGPIPE signal bhej kar band kar deta hai Is flag se program nahi crash hota bas send -1 deta hai  false return krtya hain
 9 november:
 client type kia connecttohost upload source receive response wur requestdebugfile
 helper.h mei recvall or sendall constants structs banaye
 checksum trailer class banai 
 10 nvember:
 client ka remaining complete kia .tbdg parha(openTdbg, readSnapshotAt, index aur seekg)
 session commands likhe :start ,prev,jump,next/prv,help,quit
 client ko complete kiya or prompt chala ker dekha 
 11 november:
 Server ko Phase 02 ke liye update kiya.
Ab server port argument leta hai, socket par suntaa hai upload receive karta hai
  aur chaaron passes chala kar .tdbg wapas bhejta hai
Errors ko cout ki jagah string mein rakha taake client tak bheje ja sakein
Timeline mein destructor lagaya taake memory leak na ho
 Teeno programs saath chala kar test kiye: start aur next theek chale

 12 november:
 loader balancer ke relay mei checksum verify 
 error cases ka test
 
