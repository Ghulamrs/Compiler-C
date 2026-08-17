#pragma once

#include "Backend.h"

#include <string>
#include <vector>

// The statement walk both code generators share, written once.
//
// A statement is control flow, and control flow is the same algorithm on
// every target: evaluate a truth, branch on it, place a label, jump. What
// differs is five one-line spellings - how this machine compares with zero
// and branches, how it jumps, how it writes a label - and those are the
// virtuals below. Expressions stay with the targets, because an expression
// is where the machines genuinely part ways; 'Call' most of all.
//
// This class exists because the two backends held fourteen visitors that
// backend-overlap measured as one implementation stored twice, and the pair
// had already drifted: one spelled a loop's continue label "step" and the
// other "cont", one skipped the dead jump of an else-less 'if' and the other
// emitted it. Nothing observable turned on either difference, which is the
// point - duplication does not announce which of its differences are meant.
// The walk is one text now, and a difference between targets has to be a
// primitive, where it is visibly a decision.
class Walker : public CodeGen {
public:
    void visit(const ExprStmt &n) override;
    void visit(const Block &n) override;
    void visit(const If &n) override;
    void visit(const While &n) override;
    void visit(const For &n) override;
    void visit(const DoWhile &n) override;
    void visit(const Switch &n) override;
    void visit(const Case &n) override;
    void visit(const Goto &n) override;
    void visit(const Label &n) override;
    void visit(const Conditional &n) override;
    void visit(const Comma &n) override;
    void visit(const Break &n) override;
    void visit(const Continue &n) override;

protected:
    // The five spellings a target owes the walk. Each is one or two
    // instructions; branchIfZero and branchIfNotZero read the truth that
    // genTruth left in the accumulator.
    virtual void defineLabel(const std::string &l) = 0;
    virtual void jump(const std::string &l) = 0;
    virtual void branchIfZero(const std::string &l) = 0;
    virtual void branchIfNotZero(const std::string &l) = 0;
    // Compare the switch subject in the accumulator against one case's value
    // and branch when equal - the place immediate-width limits live.
    virtual void caseBranch(long long v, const std::string &l) = 0;

    // A value's truth in the accumulator, and the names labels take. Both
    // already exist in every target; the walk only needs to call them.
    virtual void genTruth(const Expr &e) = 0;
    virtual std::string label(const char *kind, int id) const = 0;
    virtual std::string userLabel(const std::string &name) const = 0;

    // The label counter and the break/continue stack, which are the walk's
    // own state rather than any target's.
    int nextLabel() { return labels_++; }
    void resetLabels() { labels_ = 0; }
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

private:
    int labels_ = 0;
};
