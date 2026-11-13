#pragma once
#include <string>
#include <cstdint>
#include <cstddef>
#include <sys/socket.h>
using namespace std;
// Helper.h should contain the structs, functions, constants, etc. that are used more than one time in the project. For example, the structs that are used in both server and client, the constants that are used in all three files. 
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    string targetFuncName;
};
struct Snapshot; // fwd declaration;
struct TimelineNode
{
    Snapshot* data;
    TimelineNode* next;
    TimelineNode* prev;
};

// Core structs
struct Variable
{
    string name;
    int32_t value;
};
struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};
struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};
struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};
inline bool recvAll(int sock, char* buf, size_t n)
{
    // recv() can return fewer bytes than asked for, so keep calling it until exactly n bytes are read.
    // Return false if the connection closes or fails before n bytes arrive.
    size_t got=0;
    while(got<n)
    {
      ssize_t r=recv(sock,buf+ got,n-got,0);
      if(r<=0)
      {
        return false;
      }
      got+=r;
      
    }
    return true;
}
inline bool sendAll(int sock, const char* buf, size_t n)
{
      size_t sent=0;
    while(sent<n)
    {
      ssize_t r=send(sock,buf+sent,n-sent,MSG_NOSIGNAL);
      if(r<=0)
      {
        return false;
      }
      sent+=r;
      
    }
    return true;
    // send() can send fewer bytes than asked for, so keep calling it until all n bytes are sent.
    // Use the flag MSG_NOSIGNAL. Return false if the connection fails.
}

struct Crc
{
    uint32_t state=0xFFFFFFFF;


    void addBytes(const char* data,size_t count)
    
    {
        for(size_t i=0;i<count;i++)
        {
            state^=(unsigned char)data[i];
            for(int j=0;j<8;j++)
            {
                if(state & 1)
                {
                    state=(state>>1) ^ 0xEDB88320;

                }
                else{
                    state=state>>1;
                }
            }
        }
    }
        uint32_t finish() const { return ~state; }
};
inline void numberToFourBytes(uint32_t number,char out[4])
{
    out[0] = (char)(number & 0xFF);
    out[1] = (char)((number>>8 )& 0xFF);
    out[2] = (char)((number>>16 )& 0xFF);
    out[3] = (char)((number>>24 )& 0xFF);


}
inline uint32_t fourbytestonumber(const char* in)
{
    return (uint32_t)(unsigned char)in[0]|((uint32_t)(unsigned char)in[1]<<8) |((uint32_t)(unsigned char)in[2]<<16) |((uint32_t)(unsigned char)in[3]<<24);
    

}
class Trailchecker
{
    Crc Crcsofar;
    string tail;
  public:


   void feed(const char* data,size_t count,string& released)
   {
    tail.append(data,count);
    if(tail.size()>4)
    {
                    size_t extra = tail.size() - 4;
            Crcsofar.addBytes(tail.data(), extra);
            released.append(tail, 0, extra);
            tail.erase(0, extra);

    }
   }
   bool verify() const{
     if(tail.size()!=4)
     {
        return false;
     }
     return fourbytestonumber(tail.data())== Crcsofar.finish();
   }
   const string& trailerbytes() const
   {
    return tail;
   }
};
