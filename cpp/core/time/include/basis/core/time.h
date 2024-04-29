namespace basis {
namespace core {
struct Time {
    int64_t sec;
    int64_t nsec;
};

struct Realtime : public Time {


};

struct SimulatedTime : public Time {
};
}
}