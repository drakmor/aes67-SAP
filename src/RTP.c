#include <math.h>
#include "RTP.h"

#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define SERVER "239.69.128.164"
#define BUFLEN 1500  //Max length of buffer
#define PORT 5004   //The port on which to send data

struct sockaddr_in si_other;
int s, i, x, slen=sizeof(si_other);
char buf[BUFLEN];
char messageBuf[BUFLEN];
struct audioStreams *AudioStreams;

//Not the right place for some of these but it'll work for now
uint32_t sampleRate = 48000; //Samples per second
uint64_t networkLatency = 144; //In samples, I'm thinking ideally it should be 3x samples per packet(?)
uint8_t myIP[4];
uint8_t clockMaster[8];
uint8_t map = 98;

void initializeAudioStreaming(){
	initRTPSocket(); //Should be per stream since they should all have unique IPs
	AudioStreams = NULL;
}

void newAudioStream(char *name, uint8_t channels, uint64_t sessionId, uint64_t sessionVersion){
	struct RTCPstream *newStream = malloc(sizeof(struct RTCPstream));
	
	//TODO: Find a better way to set the starting time stamp
	struct timeval currenttime;
	gettimeofday(&currenttime, NULL);
	uint64_t timeInSamples = ((double)(currenttime.tv_sec+((double)currenttime.tv_usec)/1000000))*48000;
	printf("sample time: %d\n",timeInSamples);
	
	newStream->timestamp = timeInSamples;
	newStream->csrc = 1234;
	newStream->samplesPerPacket = 48;
	newStream->channels = channels;
	newStream->name = name;
	newStream->offset = 0;
	
	newStream->sessionId = sessionId;
	newStream->sessionVersion = sessionVersion;
	newStream->channelStart = 1;
	newStream->channelEnd = channels;
	newStream->map = map++;
	
	newStream->outputBufs = malloc(sizeof(struct OutputStreamBuf)*channels);
	int i;
	for(i = 0; i< channels;i++){
		newStream->outputBufs[i].outputBuf = malloc(sizeof(double)*10984320);
		newStream->outputBufs[i].head = 0;
		newStream->outputBufs[i].tail = 0;
		newStream->outputBufs[i].headTimestamp = timeInSamples;
		newStream->outputBufs[i].outputBufSize = 10984320;//TODO: make reasonable and dynamic
	}
	
	struct audioStreams *newStreamLink = malloc(sizeof(struct audioStreams));
	newStreamLink->current = newStream;
	newStreamLink->next = NULL;
	
	if(AudioStreams == NULL){	
		AudioStreams = newStreamLink;
	}else{
		struct audioStreams *streams = AudioStreams;
		while(streams->next != NULL){
			streams = streams->next;
		}
		streams->next = newStreamLink;
	}
	
}

//Should return errors if something failed to initialize
void initRTPSocket()
{
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
		printf("Failed to open socket\n");
		exit(1);
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\r\n");
        exit(1);
    }
}

void transmitTime(double time){
	char messageBuf2[BUFLEN];
	sprintf(messageBuf2,"%f",time);
	
	if (sendto(s, messageBuf2, strlen(messageBuf2), 0, (struct sockaddr *) &si_other, slen)==-1)
	{

	}
	
}
/* SINE WAVE CODE
	double factor = 48000.0/(3.14159*500);
	
				sample = sin(((double)(stream->timestamp+i))/factor)*8388607.0*.05;
				messageBuf[12+i*stream->channelsPerPacket*3+x*3+2] = sample&0x0000FF;
				messageBuf[12+i*stream->channelsPerPacket*3+x*3+1] = (sample&0x00FF00)>>8;
				messageBuf[12+i*stream->channelsPerPacket*3+x*3] = (sample&0xFF0000)>>16;
*/




void transmitRTP(struct RTCPstream *stream)
{
	//RTP header
	messageBuf[0]=128; //Version, padding, CC & M
	messageBuf[1]=99; //Stream type (from RTP map)?
	messageBuf[3]=stream->sequenceNum & 0x00FF;
	messageBuf[2]=(stream->sequenceNum & 0xFF00)>>8;
	
	messageBuf[7]=(stream->timestamp & 0x000000FF)>>0;
	messageBuf[6]=(stream->timestamp & 0x0000FF00)>>8;
	messageBuf[5]=(stream->timestamp & 0x00FF0000)>>16;
	messageBuf[4]=(stream->timestamp & 0xFF000000)>>24;
	
	messageBuf[8]=(stream->csrc & 0x000000FF)>>0;
	messageBuf[9]=(stream->csrc & 0x0000FF00)>>8;
	messageBuf[10]=(stream->csrc & 0x00FF0000)>>16;
	messageBuf[11]=(stream->csrc & 0xFF000000)>>24;

	uint32_t sample;
	
	for(i = 0;i<stream->samplesPerPacket;i++)
	{
		for(x = 0;x<stream->channels;x++){
			sample = getSampleFromBuffer(&stream->outputBufs[x],i+stream->timestamp)*8388607;
			messageBuf[12+i*stream->channels*3+x*3+2] = sample&0x0000FF;
			messageBuf[12+i*stream->channels*3+x*3+1] = (sample&0x00FF00)>>8;
			messageBuf[12+i*stream->channels*3+x*3] = (sample&0xFF0000)>>16;				
		}
	}
	
	for(x = 0;x<stream->channels;x++){
		advanceBuffer(&stream->outputBufs[x], stream->samplesPerPacket);
	}
	
	int messagelen = 12+stream->channels*stream->samplesPerPacket*3;

	//send the message
	if (sendto(s, messageBuf, messagelen, 0, (struct sockaddr *) &si_other, slen)==-1)
	{
		printf("error sending %d\n", messagelen);
	}

	stream->sequenceNum++;
	stream->timestamp += 48;
}
