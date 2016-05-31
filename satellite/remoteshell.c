/*
 * remoteshell.c
 *
 *  Created on: 05/02/2016
 *      Author: Ben Southwell
 *              Luyang Li
 */

#include <command/remoteshell.h>
#include <command/command.h> // command_run()
#include "../lib/libcsp/include/csp/csp.h"  // csp_packet_t
#include <util/log.h>
#include <stdint.h>
#include <string.h>

/* Packet structure should be as followed
<root_command|sub_command>
it is possible for arguments to be parsed
<root_command|sub_command|argv[0]>
i.e {<,a,d,c,s,|,h,k,|,1,>}
<root_command|sub_command|argv[0]|argv{1}>
..etc
it is also possible for sub_command to be a chain, i.e
<root_command|sub_command_chain|sub_command>
<root_command|sub_command_chain|sub_command|argv[0]>
*/


int remoteShell(char * command){
  int runResult;
  char * ptr = strchr(command, CRMSH_START);
  if (!ptr){
    sprintf(command, "Cannot locate the start of the packet.\n");
    return -1;
  }
  memmove(command, ptr+1, strlen(command));
  ptr = strchr(command, CRMSH_END);
  if (!ptr || !(ptr + 1)){
    sprintf(command, "Cannot locate the end of the packet.\n");
    return -1;
  }
  memset(ptr,0,1);
  ptr=strchr(command,'|');

  while (ptr != NULL){
    memset(ptr,' ',1);
    ptr=strchr(ptr+1,'|');
  }
  runResult = command_run(command);
  return runResult;
}
