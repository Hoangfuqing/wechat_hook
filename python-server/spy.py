import json
from ast import literal_eval
from socket import socket, AF_INET, SOCK_STREAM
from threading import Thread
from time import sleep
import logging

pattern = '[\u4e00-\u9fa5]'
formatter = logging.Formatter('%(asctime)s [%(threadName)s] %(levelname)s: %(message)s')
sh = logging.StreamHandler()
sh.setFormatter(formatter)
sh.setLevel(logging.DEBUG)


class WeChatSpy:
    def __init__(self, parser=None):
        self.logger = logging.getLogger(__file__)
        self.logger.addHandler(sh)
        self.logger.setLevel(logging.DEBUG)

        # socket数据处理函数
        self.__parser = parser
        self.__pid2client = {}
        self.__socket_server = socket(AF_INET, SOCK_STREAM)
        self.__socket_server.bind(("127.0.0.1", 9527))  # 绑定socket到微信端口
        self.__socket_server.listen(1)  # 开始监听微信
        t_start_server = Thread(target=self.__start_server)
        t_start_server.daemon = True
        t_start_server.name = "socket accept"
        t_start_server.start()

    def __start_server(self):
        while True:
            socket_client, client_address = self.__socket_server.accept()  # 接收微信客户端的连接
            t_socket_client_receive = Thread(target=self.receive, args=(socket_client,))
            t_socket_client_receive.name = f"client {client_address[1]}"
            t_socket_client_receive.daemon = True
            t_socket_client_receive.start()

    def __str_to_json(self, data):
        """
        把接收到的json字符串反序列化成python对象
        :param data:
        :return:
        """
        data_list = data.split(',"content":"')
        # print(data_list)
        if len(data_list) == 1:
            return literal_eval(data_list[0])
        elif len(data_list) == 2:
            data_info = data_list[0] + '}'
            data_json = literal_eval(data_info)
            content = data_list[1].strip('"}')
            data_json['content'] = content
            return data_json
        else:
            raise

    def receive(self, socket_client):
        data_str = ""
        _data_str = None
        while True:
            try:
                _data_str = socket_client.recv(4096).decode(encoding="utf-8", errors="ignore")
            except Exception as e:
                for pid, client in self.__pid2client.items():
                    if client == socket_client:
                        self.__pid2client.pop(pid)
                        return self.logger.warning(f"A WeChat process (PID:{pid}) has disconnected: {e}")
                else:
                    pid = "unknown"
                    return self.logger.warning(f"A WeChat process (PID:{pid}) has disconnected: {e}")
            if _data_str:  # 防止一次接收4096个字节，没有把所有内容都接收完，要把多次接收内容拼接起来，组成一次完成的消息内容
                data_str += _data_str
            if data_str and data_str.endswith("*88888888*"):  # 防止socket黏包
                for data in data_str.split("*88888888*"):
                    if data:
                        try:
                            data = self.__str_to_json(data)
                        except:
                            self.logger.warning("接收数据解析出错！")
                            data = None
                        if data:
                            if not self.__pid2client.get(data["pid"]) and data["type"] == 200:
                                self.__pid2client[data["pid"]] = socket_client
                                self.logger.info(f"A WeChat process (PID:{data['pid']}) successfully connected")
                            if callable(self.__parser):
                                self.__parser(data)
                data_str = ""

    def __send(self,data,pid):
        if pid:
            socket_client = self.__pid2client.get(pid)
        else:
            socket_client_list = list(self.__pid2client.values())
            socket_client = socket_client_list[0] if socket_client_list else None
        if socket_client:
            data = json.dumps(data, ensure_ascii=False)
            try:
                socket_client.send(data.encode(encoding="utf-8"))
            except Exception as e:
                for pid, v in self.__pid2client.items():
                    if v == socket_client:
                        self.__pid2client.pop(pid)
                        return self.logger.warning(f"A WeChat process (PID:{pid}) has disconnected: {e}")
                else:
                    pid = "unknown"
                    return self.logger.warning(f"A WeChat process (PID:{pid}) has disconnected: {e}")

    def run(self, background=False):
        if not background:
            while True:
                sleep(86400)

    def send_text(self, wxid, content, at_wxid="", pid=None):
        """
        发送文本消息
        :param wxid: 文本消息接收wxid（个人id或者群id）
        :param content: 文本消息内容
        :param at_wxid: 如果wxid为群wxid且需要@群成员 此参数为被@群成员wxid，以英文逗号分隔
        :param pid: 微信的进程id
        """
        if not wxid.endswith("chatroom"):
            at_wxid = ""
        data = {"code": 1, "wxid": wxid, "at_wxid": at_wxid, "content": content}
        self.__send(data, pid)
