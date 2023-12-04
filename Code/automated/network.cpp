int setupNetwork()
{
    return initializeSocket()
}

void handleError(int errorCode)
{
    if (errorCode < 0)
        netwrokError = true;
}