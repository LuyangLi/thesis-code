/*
 * server.c
 *
 *  Created on: 29/01/2010
 *      Author: johan
 */

/* This is the server file written by Johan De Claville Christiansen for GomSpace.
 
 Modification for UNSW QB50's code is between Line 348 to Line 394 */
 
		/* Added by Luyang: tasks with csp before being handover to csp service handler.*/
		case OBC_PORT_CUSTOM_REMOTE_SHELL: {

			remoteShell_packet_t * commands = packet->data;

			int16_t result = remoteShell(commands->command_text);
			commands->returned = csp_hton16(result);
			memcpy(packet->data, commands, sizeof(remoteShell_packet_t));
			packet->length = sizeof(remoteShell_packet_t);
			if (!csp_send(conn, packet, 5000))
				csp_buffer_free(packet);
			break;
		}

		case OBC_PORT_FTP_LIST: {

			remoteShell_packet_t * commands = packet->data;
			int16_t start = csp_ntoh16(commands->returned);
			int16_t result = fs_ls_to_sd(commands->command_text, start);
			commands->returned = csp_hton16(result);
			memcpy(packet->data, commands, sizeof(remoteShell_packet_t));
			packet->length = sizeof(remoteShell_packet_t);
			if (!csp_send(conn, packet, 5000))
				csp_buffer_free(packet);
			break;
		}


		case OBC_PORT_BEACON: {
			beacon_config_t * config = packet->data;
			switch (config->tasks) {
			case 1:
				set_beacon_config(config);
				break;
			default:
				get_beacon_config(config);
				break;
			}
			memcpy(packet->data, config, sizeof(beacon_config_t));
			if (!csp_send(conn, packet, 5000))
				csp_buffer_free(packet);
			break;
		}

		/* Custom tasks Ended.*/
		
