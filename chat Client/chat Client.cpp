#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <memory>
#include <iostream>

#include "Socket.h"
#include "Util.h"

using namespace std;

const int ClientNum = 10;

int main()
{
	SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

	using namespace std::chrono_literals;

	recursive_mutex mutex; // 아래 변수들을 보호한다.
	vector<shared_ptr<thread>> threads;
	int64_t totalReceivedBytes = 0;
	int connectedClientCount = 0;


	for (int i = 0; i < ClientNum; i++)
	{
		// TCP 연결을 하고 송수신을 하는 스레드를 생성한다. 무한 루프를 돈다.
		shared_ptr<thread> th = make_shared<thread>([&connectedClientCount, &mutex, &totalReceivedBytes]()
			{
				try
				{
					Socket tcpSocket(SocketType::Tcp);
					tcpSocket.Bind(Endpoint::Any);
					tcpSocket.Connect(Endpoint("127.0.0.1", 11021));

					{
						lock_guard<recursive_mutex> lock(mutex);
						connectedClientCount++;
					}

					string receivedData;

					while (true)
					{
						const char* dataToSend = "hello world.";
						// 원칙대로라면 TCP 스트림 특성상 일부만 송신하고 리턴하는 경우도 고려해야 하나,
						// 지금은 독자의 이해가 우선이므로, 생략하도록 한다.
						tcpSocket.Send(dataToSend, strlen(dataToSend) + 1);
						int receiveLength = tcpSocket.Receive();
						tcpSocket.Print();
						if (receiveLength <= 0)
						{
							// 소켓 연결에 문제가 생겼습니다. 루프를 끝냅니다.
							break;
						}
						lock_guard<recursive_mutex> lock(mutex);
						totalReceivedBytes += receiveLength;
					}
				}
				catch (std::exception& e)
				{
					lock_guard<recursive_mutex> lock(mutex);
					cout << "A TCP socket work failed : " << e.what() << endl;
				}

				lock_guard<recursive_mutex> lock(mutex);
				connectedClientCount--;
			});

		lock_guard<recursive_mutex> lock(mutex);
		threads.push_back(th);
	}

	// 메인 스레드는 매 초마다 총 송수신량을 출력한다.
	while (true)
	{
		{
			lock_guard<recursive_mutex> lock(mutex);
			cout << "Total echoed bytes: " << (uint64_t)totalReceivedBytes << ", thread count: " << connectedClientCount << endl;
		}
		this_thread::sleep_for(2s);
	}


	return 0;
}
