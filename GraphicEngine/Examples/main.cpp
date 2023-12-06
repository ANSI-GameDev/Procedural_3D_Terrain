#include <memory>
#include <windows.h>

#include "CloudDemo.h"
#include "GrassDemo.h"

using namespace std;

int main(int argc, char *argv[]) {

    std::unique_ptr<ansi::AppBase> app;

    if (argc < 2) {
        cout << "Please specify the example number" << endl;
        return -1;
    }

    app = make_unique<ansi::GrassDemo>();
    if (!app->Initialize()) {
        cout << "Initialization failed." << endl;
        return -1;
    }

    return app->Run();

}
