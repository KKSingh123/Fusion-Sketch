#ifndef TAILOREDSKETCH_CODE_SKETCH_H
#define TAILOREDSKETCH_CODE_SKETCH_H

class Sketch {
public:
    virtual ~Sketch() = default;
    virtual void Insert(const char* str)=0;
    virtual int Query(const char* str)=0;
};

#endif //TAILOREDSKETCH_CODE_SKETCH_H
