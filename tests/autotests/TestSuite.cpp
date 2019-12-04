#include "TestSuite.h"

TestSuite::TestSuite(QObject* parent)
    : QObject(parent)
{
    suite().push_back(this);
}

std::vector<QObject*> & TestSuite::suite()
{
    static std::vector<QObject*> objects;
    return objects;
}
