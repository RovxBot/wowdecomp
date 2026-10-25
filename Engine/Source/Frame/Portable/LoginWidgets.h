#ifndef WOWDECOMP_NATIVE_LOGIN_WIDGETS_H
#define WOWDECOMP_NATIVE_LOGIN_WIDGETS_H
namespace Frame { namespace Native {
// Lives inside Scripts and outside every original login frame.
class LoginWidgets final {
public:
    LoginWidgets();
    ~LoginWidgets();
    LoginWidgets(const LoginWidgets&) = delete;
    LoginWidgets& operator=(const LoginWidgets&) = delete;
};
}}
#endif
