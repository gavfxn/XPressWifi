#include "twn4.sys.h"
#include "apptools.h"

const unsigned char AppManifest[] = { EXECUTE_APP, 1, EXECUTE_APP_ALWAYS, TLV_END };

int main(void)
{
    while (true)
    {
        // Intentionally empty - minimal test app
        Beep(25, 1000, 100, 0);
    }
    return 0;
}