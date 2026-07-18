#include "application.h"

int main() {
    Application app;
    if (!app.init()) return 1;
    app.run();
    app.shutdown();
    return 0;
}
