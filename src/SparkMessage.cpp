/*
 * SparkDataControl.cpp
 *
 *  Created on: 19.08.2021
 *      Author: stangreg
 */

#include "SparkMessage.h"


SparkMessage::SparkMessage(){
	data = {};
	split_data8={};
	split_data7={};
	cmd=0;
	sub_cmd=0;
}

void SparkMessage::start_message (byte _cmd, byte _sub_cmd){
	cmd = _cmd;
	sub_cmd = _sub_cmd;
	data = {};
	split_data8 = {};
	split_data7 = {};
	final_message = {};
};

std::vector<ByteVector> SparkMessage::end_message(){

	// determine how many chunks there are
	int data_len = data.size();
	int num_chunks = int ((data_len + 0x7f) / 0x80 );


	// split the data into chunks of maximum 0x80 bytes (still 8 bit bytes)
	// and add a chunk sub-header if a multi-chunk message

	for (int this_chunk=0; this_chunk <num_chunks; this_chunk++){
		int chunk_len = min (0x80, data_len - (this_chunk * 0x80));
		ByteVector data8;
		if (num_chunks > 1){
			// we need the chunk sub-header
			data8.push_back(num_chunks);
			data8.push_back(this_chunk);
			data8.push_back(chunk_len);
		}
		else{
			data8 = {};
		}
		data8.insert(data8.end(),data.begin()+ (this_chunk * 0x80), data.begin() + (this_chunk * 0x80 + chunk_len));
		split_data8.push_back(data8);
	}

	// now we can convert this to 7-bit data format with the 8-bits byte at the front
	// so loop over each chunk
	// and in each chunk loop over every sequence of (max) 7 bytes
	// and extract the 8th bit and put in 'bit8'
	// and then add bit8 and the 7-bit sequence to data7

	for (auto chunk : split_data8){

		int chunk_len = chunk.size();
		int num_seq = int ((chunk_len + 6) / 7);
		ByteVector bytes7 = {};


		for (int this_seq = 0; this_seq < num_seq; this_seq++){
			int seq_len = min (7, chunk_len - (this_seq * 7));
			byte bit8 = 0;
			ByteVector seq = {};
			for (int ind = 0; ind < seq_len; ind++){
				byte dat = chunk[this_seq * 7 + ind];
				if ((dat & 0x80) == 0x80){
					bit8 |= (1<<ind);
				}
				dat &= 0x7f;

				seq.push_back((byte)dat);
			}

			bytes7.push_back(bit8);
			bytes7.insert(bytes7.end(), seq.begin(), seq.end());
		}
		split_data7.push_back(bytes7);
	}



	// now we can create the final message with the message header and the chunk header
	ByteVector block_header = {'\x01','\xfe','\x00','\x00','\x53','\xfe'};
	ByteVector block_filler = {'\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00'};
	ByteVector chunk_header = {'\xf0','\x01','\x3a','\x15'};

	for (auto chunk: split_data7){
		int block_size = chunk.size() + 16 + 6 + 1;

		ByteVector header = block_header;
		// Build header
		header.push_back(block_size);
		header.insert(header.end(), block_filler.begin(), block_filler.end());
		header.insert(header.end(), chunk_header.begin(), chunk_header.end());
		header.push_back(cmd);
		header.push_back(sub_cmd);
		byte trailer = '\xf7';
		ByteVector full_chunk = header;
		full_chunk.insert(full_chunk.end(), chunk.begin(), chunk.end());
		full_chunk.push_back(trailer);
		final_message.push_back(full_chunk);
	}

	return final_message;

}

void SparkMessage::add_bytes(ByteVector bytes_8){
	for (byte by: bytes_8){
		data.push_back(by);
	}
}

void SparkMessage::add_byte(byte by){
	data.push_back(by);
}

void SparkMessage::add_prefixed_string(std::string pack_str){
	int str_length = pack_str.size();
	ByteVector byte_pack;
	byte_pack.push_back((byte)str_length);
	byte_pack.push_back((byte)(str_length + 0xa0));
	std::copy(pack_str.begin(), pack_str.end(), std::back_inserter<ByteVector>(byte_pack));
	add_bytes (byte_pack);
}


void SparkMessage::add_string(std::string pack_str){
	int str_length = pack_str.size();
	ByteVector byte_pack;
	byte_pack.push_back((byte)(str_length + 0xa0));
	std::copy(pack_str.begin(), pack_str.end(), std::back_inserter<ByteVector>(byte_pack));
	add_bytes (byte_pack);
}

void SparkMessage::add_long_string(std::string pack_str){
	int str_length = pack_str.size();
	ByteVector byte_pack;
	byte_pack.push_back(('\xd9'));
	byte_pack.push_back((byte)str_length);
	std::copy(pack_str.begin(), pack_str.end(), std::back_inserter<ByteVector>(byte_pack));
	add_bytes(byte_pack);
}

void SparkMessage::add_float (float flt){

	union {
		float float_variable;
		byte temp_array[4];
	} u;
	// Overwrite bytes of union with float variable
	u.float_variable = flt;
	ByteVector byte_pack;
	byte_pack.push_back((byte)0xca);
	// Assign bytes to input array
	for(int i=3; i>=0; i--){
		byte_pack.push_back(u.temp_array[i]);
	}

	add_bytes(byte_pack);
}

void SparkMessage::add_onoff (boolean enable){
	byte b;
	if (enable == true){
		b = '\xc3';
	}
	else{
		b = '\xc2';
	}
	add_byte(b);
}

std::vector<ByteVector> SparkMessage::get_current_preset_num(){
	// hardcoded message
	std::vector<ByteVector> msg;
	ByteVector msg_vec = {0x01, 0xfe, 0x00, 0x00, 0x53, 0xfe,
			0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xf0, 0x01, 0x08, 0x00, 0x02, 0x10, 0xf7}; // had an extra 0x79 at the end, likely not needed
	msg.push_back(msg_vec);
	return msg;
}

std::vector<ByteVector> SparkMessage::get_current_preset(){
	// hardcoded message
	std::vector<ByteVector> msg;
	ByteVector msg_vec = {0x01, 0xfe, 0x00, 0x00, 0x53, 0xfe,
			0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xf0, 0x01, 0x0a, 0x01, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0xf7};

	msg.push_back(msg_vec);
	return msg;
}

std::vector<ByteVector> SparkMessage::change_effect_parameter (std::string pedal, int param, float val){
	cmd = '\x01';
	sub_cmd = '\x04';

	start_message (cmd, sub_cmd);
	add_prefixed_string (pedal);
	add_byte((byte)param);
	add_float(val);
	return end_message();

}

std::vector<ByteVector> SparkMessage::change_effect (std::string pedal1, std::string pedal2){
	cmd = '\x01';
	sub_cmd = '\x06';

	start_message (cmd, sub_cmd);
	add_prefixed_string (pedal1);
	add_prefixed_string (pedal2);
	return end_message();


}

std::vector<ByteVector> SparkMessage::change_hardware_preset (int preset_num){
	cmd = '\x01';
	sub_cmd = '\x38';

	start_message (cmd, sub_cmd);
	add_byte((byte)0);
	add_byte((byte)preset_num-1);
	return end_message();

}

std::vector<ByteVector> SparkMessage::turn_effect_onoff (std::string pedal, boolean enable){
	cmd = '\x01';
	sub_cmd = '\x15';

	start_message (cmd, sub_cmd);
	add_prefixed_string (pedal);
	add_onoff(enable);
	return end_message();
}

std::vector<ByteVector> SparkMessage::create_preset (Preset preset_data){
	cmd = '\x01';
	sub_cmd = '\x01';

	int this_chunk = 0;

	start_message (cmd, sub_cmd);
	add_byte('\x00');
	add_byte('\x7f');
	add_long_string (preset_data.uuid);
	add_string (preset_data.name);
	add_string (preset_data.version);
	std::string descr = preset_data.description;
	if (descr.size() > 31){
		add_long_string (descr);
	}
	else{
		add_string (descr);
	}
	add_string (preset_data.icon);
	add_float (preset_data.bpm);
	add_byte ((byte)(0x90 + 7));        // 7 pedals
	for (int i=0; i<7; i++){
		Pedal curr_pedal = preset_data.pedals[i];
		add_string (curr_pedal.name);
		add_onoff(curr_pedal.isOn);
		std::vector<Parameter> curr_pedal_params = curr_pedal.parameters;
		int num_p = curr_pedal_params.size();
		add_byte ((byte)(num_p + 0x90));
		for (int p=0; p<num_p; p++){
			add_byte((byte)p);
			add_byte((byte)'\x91');
			add_float (curr_pedal_params[p].value);
		}
	}
	add_byte ((byte)(preset_data.filler));
	return end_message ();
}

// This prepares a message to send an acknowledgement to Spark App
std::vector<ByteVector> SparkMessage::send_ack(byte seq, byte cmd) {
	std::vector<ByteVector> ack_cmd;
	ack_cmd.clear();
	ByteVector ack = { 0x01, 0xfe, 0x00, 0x00, 0x41, 0xff, 0x17, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x01 };
	ack.push_back(seq);
	ack.push_back(0x00);
	ack.push_back(0x04);
	ack.push_back(cmd);
	ack.push_back(0xf7);

	ack_cmd.push_back(ack);
	return ack_cmd;
}

