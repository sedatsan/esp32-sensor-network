#pragma once

namespace sn::hal {

class IOutputPin {
public:
    virtual ~IOutputPin() = default;
    virtual void write(bool level) = 0;
};

class IInputPin {
public:
    virtual ~IInputPin() = default;
    [[nodiscard]] virtual bool read() const = 0;
};

}  // namespace sn::hal
