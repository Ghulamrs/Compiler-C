#pragma once

#include "Backend.h"
#include "Dwarf.h"

#include <cstddef>
#include <string>
#include <vector>

class Source;

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

    // Where the statements came from, for a line table. Null unless -g asked
    // for one, which is the whole of what turns this off.
    void setLineSource(const Source *s, const std::string &dir) override {
        lines_ = s;
        compDir_ = dir;
    }

    // The blocks of the function last walked, bounded by the labels the walk
    // placed. Read by the target once the body is out, and handed to the
    // DWARF writer as it stands.
    const std::vector<DwarfBlock> &blocks() const { return blocks_; }

protected:
    // Say where the statement about to be walked was written. The resolving
    // lives here because it is the same question on every target; what a
    // target owes is emitLoc, which is one line of its own assembly.
    //
    // Every statement is marked, including a second statement on a line
    // already marked: the assembler folds rows that share an address, and a
    // mark suppressed here could not be recovered by anything downstream.
    void markLine(const Stmt &n);
    // The same mark for a place that is not a statement - a function's own
    // line, so a breakpoint on its name lands on its first instruction.
    void markLine(std::size_t pos);
    const Source *lineSource() const { return lines_; }
    const std::string &compDir() const { return compDir_; }
    virtual void emitLoc(int file, int line, int column) { (void)file; (void)line; (void)column; }

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
    // How much this target has written so far. Only differences matter, and
    // only within one function - it is how the walk tells a block that emitted
    // instructions from one that emitted none. Not const, because measuring a
    // stream's position is not.
    virtual std::size_t emittedSize() = 0;

    // Start a function's blocks from the parent list the parser built. The
    // labels are filled in as the walk reaches each block; a block the walk
    // never reaches keeps empty ones, and the writer says what it does then.
    void resetBlocks(const std::vector<int> &parents);
    // A block's instructions begin and end here. Both do nothing without -g:
    // a label placed for a debugger has no business in output nobody asked to
    // debug, and the byte-for-byte fingerprint of every target says so.
    void openBlock(int scope);
    void closeBlock(int scope);

    // The label counter and the break/continue stack, which are the walk's
    // own state rather than any target's.
    int nextLabel() { return labels_++; }
    void resetLabels() { labels_ = 0; }
    struct JumpTargets { std::string brk; std::string cont; };
    std::vector<JumpTargets> jumps_;

private:
    int labels_ = 0;
    const Source *lines_ = nullptr;
    std::string compDir_;
    std::vector<DwarfBlock> blocks_;
    // What has been written that is not an instruction: the .loc directives,
    // and the block labels themselves. Subtracting it is what makes a block
    // holding nothing but another empty block count as empty too.
    std::size_t notCode_ = 0;
    struct Mark { std::size_t size; std::size_t notCode; };
    std::vector<Mark> marks_;
};
