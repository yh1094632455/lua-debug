#include <stdio.h>	    
#include <fcntl.h>
#include <io.h>	  
#include <vector>
#include <debugger/client/stdinput.h>
#include <debugger/client/launch.h>
#include <debugger/client/attach.h>
#include <debugger/client/server.h>
#include <base/util/unicode.h>
#include <base/util/format.h>
#include <base/filesystem.h>
#include <debugger/capabilities.h>
#include <debugger/io/helper.h>
#include <debugger/io/namedpipe.h>

static void response_initialized(stdinput& io, vscode::rprotocol& req)
{
	vscode::wprotocol res;

	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(1);
		res("command").String("initialize");
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(true);
		vscode::capabilities(res);
	}
	vscode::io_output(&io, res);
}

static void response_error(stdinput& io, vscode::rprotocol& req, const char *msg)
{
	vscode::wprotocol res;
	for (auto _ : res.Object())
	{
		res("type").String("response");
		res("seq").Int64(2);
		res("command").String(req["command"]);
		res("request_seq").Int64(req["seq"].GetInt64());
		res("success").Bool(false);
		res("message").String(msg);
	}
	vscode::io_output(&io, res);
	exit(-1);
}

static int stoi_nothrow(std::string const& str)
{
	try {
		return std::stoi(str);
	}
	catch (...) {
	}
	return 0;
}

static void sleep() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static uint16_t wait_ok(server& s) {
	uint16_t port = 0;
	for (int i = 0; i < 10; ++i) {
		port = s.get_port();
		if (port) {
			return port;
		}
		sleep();
		s.update();
	}
	return 0;
}

bool create_process_with_debugger(vscode::rprotocol& req, const std::wstring& port);

void io_send(vscode::io::base* io, const vscode::rprotocol& rp)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	rp.Accept(writer);
	io->output(buffer.GetString(), buffer.GetSize());
}


static int run_createprocess_then_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	std::unique_ptr<attach> attach_;
	auto port = base::format(L"vscode-lua-debug-%d", GetCurrentProcessId());
	if (!create_process_with_debugger(req, port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}

	vscode::io::namedpipe pipe;
	if (!pipe.open_server(port)) {
		response_error(io, req, "Launch failed");
		return -1;
	}
	io_send(&pipe, init);
	io_send(&pipe, req);
	for (;; sleep()) {
		io.update(10);
		pipe.update(10);
		std::string buf;
		while (io.input(buf)) {
			pipe.output(buf.data(), buf.size());
		}
		while (pipe.input(buf)) {
			io.output(buf.data(), buf.size());
		}
	}
	return 0;
}

static int run_attach(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req)
{
	attach attach(io);
	auto& args = req["arguments"];
	std::string ip = args.HasMember("ip") ? args["ip"].Get<std::string>() : "127.0.0.1";
	uint16_t port = args.HasMember("port") ? args["port"].GetUint() : 4278;
	attach.connect(net::endpoint(ip, port));
	attach.send(init);
	attach.send(req);
	for (;; sleep()) {
		attach.update();
	}
	return 0;
}

int main()
{
	MessageBox(0, 0, 0, 0);
	_setmode(_fileno(stdout), _O_BINARY);
	setbuf(stdout, NULL);

	stdinput io;
	vscode::rprotocol init;

	for (;; sleep()) {
		vscode::rprotocol req = vscode::io_input(&io);
		if (req.IsNull()) {
			continue;
		}
		if (req["type"] != "request") {
			return -1;
		}
		if (init.IsNull()) {
			if (req["command"] == "initialize") {
				response_initialized(io, req);
				init = std::move(req);
				init.AddMember("__norepl", true, init.GetAllocator());
			}
			else {
				response_error(io, req, "not initialized");
				return -1;
			}
		}
		else {
			if (req["command"] == "launch") {
				auto& args = req["arguments"];
				if (args.HasMember("runtimeExecutable")) {
					return run_createprocess_then_attach(io, init, req);
				}
				else {
					return run_launch(io, init, req);
				}
			}
			else if (req["command"] == "attach") {
				return run_attach(io, init, req);
			}
		}
	}
}