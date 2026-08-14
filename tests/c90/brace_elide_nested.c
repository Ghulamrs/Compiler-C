struct P{int x,y;}; struct Q{struct P p; int z;}; int main(void){ struct Q q={1,2,3}; return q.z-3; }
