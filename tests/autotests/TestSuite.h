#include <QObject>
#include <vector>

/** \brief Base class for the test suite runner.
 *  Taken from: https://alexhuszagh.github.io/2016/using-qttest-effectively/
 */
class TestSuite: public QObject
{
public:
     TestSuite(QObject* parent = nullptr);

     static std::vector<QObject*> & suite();
};
