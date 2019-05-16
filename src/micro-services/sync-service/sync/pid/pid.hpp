/* Code from : https://gist.github.com/bradley219/5373998 */

#ifndef _PID_HPP_
#define _PID_HPP_

class PIDImpl;
class PID
{
    public:
        // Kp -  proportional gain
        // Ki -  Integral gain
        // Kd -  derivative gain
        // dt -  loop interval time
        // max - maximum value of manipulated variable
        // min - minimum value of manipulated variable
        PID( double dt, double max, double min, double Kp, double Kd, double Ki );
        ~PID();

        // Returns the manipulated variable given a setpoint and current process value
        double calculate( double setpoint, double pv );

    private:
        PIDImpl *pimpl;
};

#endif