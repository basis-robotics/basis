class UnitBase {
    public:
        UnitBase() {}
        virtual ~UnitBase() {}
        virtual void Update() = 0;
    private:
        // callbacks
};