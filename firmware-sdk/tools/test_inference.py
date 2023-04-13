import time
import serial
import os
import sys
import struct
import binascii

def encode_and_send(string, ser):
    array_to_write = (string.encode())
    ser.write(array_to_write)
    print("Sent: {} Size: {}".format(array_to_write, len(array_to_write)))

def await_response_exact(response, ser):
    data_in = b""
    while data_in != response.encode():
        data_in = ser.readline()
        print(data_in)
        if data_in == b"TIMEOUT\r\n":
            break
    return data_in.decode()

def await_response(response, ser):
    data_in = b""
    while not response.encode() in data_in:
        data_in = ser.readline()
        print(data_in)
        if data_in == b"TIMEOUT\r\n":
            break
    return data_in.decode()

def base64_encode(features):
    feature_byte_array = struct.pack('@'+'f'*len(features), *features)
    res = binascii.b2a_base64(feature_byte_array, newline=False)

    print(len(features))
    print(len(features*4))
    print(features)
    print(str(res)[2:-1])
    return str(res)[2:-1]

def send_uart(data, raw_data_len, ser, sim_timeout=False):

    data_sent = 0

    time.sleep(2)

    encode_and_send("AT\r", ser)
    response = await_response_exact("> ", ser)

    encode_and_send("AT+RUNIMPULSESTATIC=n,{}\r".format(raw_data_len), ser)
    response = await_response("OK", ser)

    chunk_size = int(''.join(filter(str.isdigit, response)))
    print("Chunk size is {}".format(chunk_size))

    data_modulo = len(data) % chunk_size
    if data_modulo:
        # pad data to be multiple of chunk size with '='
        print("Data size before padding {}".format(len(data)) )
        data = data + "="*(chunk_size-data_modulo)
        print("Data size after padding {}".format(len(data)))

    while data_sent < len(data):
        encode_and_send("{}".format(data[data_sent:data_sent + chunk_size]), ser)
        time.sleep(0.01)
        data_sent += chunk_size
        print("Total sent: {}".format(data_sent))
        response = await_response("OK", ser)

        if sim_timeout:
            time.sleep(0.1)
        if response == "TIMEOUT\r\n":
            print("Data send time out. Terminating...")
            ser.close()
            sys.exit(1)

    response = await_response_exact("END OUTPUT\r\n", ser)
    ser.close()

if __name__ == "__main__":
    ser = serial.Serial(sys.argv[2], 115200, timeout=0.050)
    with open(sys.argv[1],'r') as f:
        data = f.read()
        if 'image' in sys.argv[1]:
            data = [float(int(num,16)) for num in data.split(',')]
        else:
            data = [float(num) for num in data.split(',')]
    encoded_data = base64_encode(data)
    send_uart(encoded_data, len(data), ser, sim_timeout=False)