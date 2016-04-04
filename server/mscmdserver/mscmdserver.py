#!/usr/bin/env python
import SimpleHTTPServer
import SocketServer
import urlparse
import subprocess
import os
import sys
import ConfigParser
import shutil

class ConfigReader(object):
    def __init__(self, **kwargs):
        self.defaults = kwargs

    def read(self, path):
        reader = ConfigParser.ConfigParser(allow_no_value=True, defaults=self.defaults)
        reader.read(path)
        return reader

class MyRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_GET(self): #handle a GET request
        mountainsort_exe        = self.cfg("mountainsort_exe")
        mdaserver_basepath            = self.cfg("mdaserver_basepath")
        remote_name   = self.cfg("remote_name")
        processor_name=self.query("processor")
        keys0=self.query_keys()
        print(remote_name)
        cmd = mountainsort_exe+" "+processor_name
        for j in range(0,len(keys0)):
            if not keys0[j]=="processor":
                val=self.query(keys0[j])
                val=val.replace("remote://"+remote_name,mdaserver_basepath)
                cmd=cmd+" --"+keys0[j]+"="+val
        (str,exit_code)=self.call_and_read_output(cmd)
        if not exit_code:
            self.send_plain_text(str)
        else:
            self.send_plain_text("ERROR: "+str+"\n<br>"+cmd)
        return

    def cfg(self, key, section = 'General'):
        return self.config.get(section, key)

    def query_keys(self): #for convenience    
        parts=urlparse.urlparse(self.path)
        query=urlparse.parse_qs(parts.query)
        return query.keys()

    def query(self,field,defaultval=""): #for convenience
    	parts=urlparse.urlparse(self.path)
    	query=urlparse.parse_qs(parts.query)
    	tmp=query.get(field,[defaultval])
        return tmp[0] if tmp else ""

    def call_and_read_output(self,cmd): #make a system call and return the output string and exit code
    	print(cmd)
    	process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    	exit_code = process.wait()
    	(out,err)=process.communicate()
    	process.stderr.close()
    	process.stdout.close()
        ret = "ERROR: {}".format(out) if exit_code else out

    	print(out)
    	return (out,exit_code)

    def mkdir_if_needed(self,path): #for convenience
		if not os.path.exists(path):
			os.makedirs(path)

    def send_plain_text(self,txt): #send a plain text response
		self.send_response(200)
		self.send_header("Content-type", "text/html")
		self.send_header("Content-length", len(txt))
		self.end_headers()
		self.wfile.write(txt)
	

class MyTCPServer(SocketServer.TCPServer):
    allow_reuse_address = True

def main():
    config_fname='mscmdserver.cfg'
    example_config_fname='mscmdserver.example.cfg'
    if not os.path.isfile(config_fname):
        shutil.copyfile(example_config_fname,config_fname)
        print("Please edit the configuration file "+config_fname+" and then re-run this program")
        return
    Handler = MyRequestHandler
    port = sys.argv[1] if sys.argv[1:] else "8001"
    config = ConfigReader(port=port).read(config_fname)
    Handler.config = config
    server = MyTCPServer(('0.0.0.0', int(config.get('General', 'port'))), Handler)
    server.serve_forever()

if __name__ == "__main__":
    main()
