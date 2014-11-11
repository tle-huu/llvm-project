//===-- RegisterContextLLDB.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_RegisterContextLLDB_h_
#define lldb_RegisterContextLLDB_h_

#include <vector>

#include "lldb/lldb-private.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Symbol/SymbolContext.h"
#include "UnwindLLDB.h"

namespace lldb_private {

class UnwindLLDB;

class RegisterContextLLDB : public lldb_private::RegisterContext
{
public:
    typedef std::shared_ptr<RegisterContextLLDB> SharedPtr;

    RegisterContextLLDB (lldb_private::Thread &thread,
                         const SharedPtr& next_frame,
                         lldb_private::SymbolContext& sym_ctx,
                         uint32_t frame_number, lldb_private::UnwindLLDB& unwind_lldb);

    ///
    // pure virtual functions from the base class that we must implement
    ///

    virtual
    ~RegisterContextLLDB () { }

    virtual void
    InvalidateAllRegisters ();

    virtual size_t
    GetRegisterCount ();

    virtual const lldb_private::RegisterInfo *
    GetRegisterInfoAtIndex (size_t reg);

    virtual size_t
    GetRegisterSetCount ();

    virtual const lldb_private::RegisterSet *
    GetRegisterSet (size_t reg_set);

    virtual bool
    ReadRegister (const lldb_private::RegisterInfo *reg_info, lldb_private::RegisterValue &value);

    virtual bool
    WriteRegister (const lldb_private::RegisterInfo *reg_info, const lldb_private::RegisterValue &value);

    virtual bool
    ReadAllRegisterValues (lldb::DataBufferSP &data_sp);

    virtual bool
    WriteAllRegisterValues (const lldb::DataBufferSP &data_sp);

    virtual uint32_t
    ConvertRegisterKindToRegisterNumber (lldb::RegisterKind kind, uint32_t num);

    bool
    IsValid () const;

    bool
    IsTrapHandlerFrame () const;

    bool
    GetCFA (lldb::addr_t& cfa);

    bool
    GetStartPC (lldb::addr_t& start_pc);

    bool
    ReadPC (lldb::addr_t& start_pc);

private:

    //--------------------------------------------------------------------
    /// A convenience class for RegisterContextLLDB which can convert between
    /// different register kinds.
    ///
    /// This ends up being a common operation in RegisterContextLLDB as we try
    /// to bridge between the different register numbering systems -- having a
    /// simple object to enclose all of that conversion, and cache results so
    /// we don't re-fetch, simplifies the code significantly.
    //--------------------------------------------------------------------
    class RegisterNumber {
    public:
        RegisterNumber (lldb_private::Thread &thread, lldb::RegisterKind kind, uint32_t num) :
            m_reg_ctx_sp (thread.GetRegisterContext()),
            m_regnum (num),
            m_kind (kind),
            m_kind_regnum_map (),
            m_name ("")
        {
            if (m_reg_ctx_sp.get())
            {
                const lldb_private::RegisterInfo *reginfo = m_reg_ctx_sp->GetRegisterInfoAtIndex (GetAsKind (lldb::eRegisterKindLLDB));
                if (reginfo && reginfo->name)
                {
                    m_name = reginfo->name;
                }
            }
        }

        // This constructor plus the init() method below allow for the placeholder
        // creation of an invalid object initially, possibly to be filled in.  It
        // would be more consistent to have three Set* methods to set the three
        // data that the object needs.
        RegisterNumber () :
            m_reg_ctx_sp(),
            m_regnum (LLDB_INVALID_REGNUM),
            m_kind (lldb::kNumRegisterKinds),
            m_kind_regnum_map (),
            m_name (nullptr)
        {
        }

        void
        init (lldb_private::Thread &thread, lldb::RegisterKind kind, uint32_t num)
        {
            m_reg_ctx_sp = thread.GetRegisterContext();
            m_regnum = num;
            m_kind = kind;
            if (m_reg_ctx_sp.get())
            {
                const lldb_private::RegisterInfo *reginfo = m_reg_ctx_sp->GetRegisterInfoAtIndex (GetAsKind (lldb::eRegisterKindLLDB));
                if (reginfo && reginfo->name)
                {
                    m_name = reginfo->name;
                }
            }
        }

        const RegisterNumber &
        operator = (const RegisterNumber &rhs)
        {
            m_reg_ctx_sp = rhs.m_reg_ctx_sp;
            m_regnum = rhs.m_regnum;
            m_kind = rhs.m_kind;
            for (auto it : rhs.m_kind_regnum_map)
                m_kind_regnum_map[it.first] = it.second;
            m_name = rhs.m_name;
            return *this;
        }

        bool
        operator == (RegisterNumber &rhs)
        {
            if (IsValid() != rhs.IsValid())
                return false;

            if (m_kind == rhs.m_kind)
            {
                if (m_regnum == rhs.m_regnum)
                    return true;
                else
                    return false;
            }

            uint32_t rhs_regnum = rhs.GetAsKind (m_kind);
            if (rhs_regnum != LLDB_INVALID_REGNUM)
            {
                if (m_regnum == rhs_regnum)
                    return true;
                else
                    return false;
            }
            uint32_t lhs_regnum = GetAsKind (rhs.m_kind);
            {
                if (lhs_regnum == rhs.m_regnum)
                    return true;
                else 
                    return false;
            }
            return false;
        }

        bool
        IsValid () const
        {
            return m_regnum != LLDB_INVALID_REGNUM;
        }

        uint32_t
        GetAsKind (lldb::RegisterKind kind)
        {
            if (m_regnum == LLDB_INVALID_REGNUM)
                return LLDB_INVALID_REGNUM;

            if (kind == m_kind)
                return m_regnum;

            Collection::iterator iter = m_kind_regnum_map.find (kind);
            if (iter != m_kind_regnum_map.end())
            {
                return iter->second;
            }
            uint32_t output_regnum = LLDB_INVALID_REGNUM;
            if (m_reg_ctx_sp 
                && m_reg_ctx_sp->ConvertBetweenRegisterKinds (m_kind, m_regnum, kind, output_regnum)
                && output_regnum != LLDB_INVALID_REGNUM)
            {
                m_kind_regnum_map[kind] = output_regnum;
            }
            return output_regnum;
        }

        const char *
        GetName ()
        {
            return m_name;
        }

    private:
        typedef std::map<lldb::RegisterKind, uint32_t> Collection; 

        lldb::RegisterContextSP m_reg_ctx_sp;
        uint32_t                m_regnum;
        lldb::RegisterKind      m_kind;
        Collection              m_kind_regnum_map;
        const char              *m_name;
    };

    enum FrameType
    {
        eNormalFrame,
        eTrapHandlerFrame,
        eDebuggerFrame,  // a debugger inferior function call frame; we get caller's registers from debugger
        eSkipFrame,      // The unwind resulted in a bogus frame but may get back on track so we don't want to give up yet
        eNotAValidFrame  // this frame is invalid for some reason - most likely it is past the top (end) of the stack
    };

    // UnwindLLDB needs to pass around references to RegisterLocations
    friend class UnwindLLDB;


    // Returns true if we have an unwind loop -- the same stack frame unwinding 
    // multiple times.
    bool
    CheckIfLoopingStack ();

    // Indicates whether this frame is frame zero -- the currently
    // executing frame -- or not.
    bool
    IsFrameZero () const;

    void
    InitializeZerothFrame ();

    void
    InitializeNonZerothFrame();

    SharedPtr
    GetNextFrame () const;

    SharedPtr
    GetPrevFrame () const;

    // A SkipFrame occurs when the unwind out of frame 0 didn't go right -- we've got one bogus frame at frame #1.
    // There is a good chance we'll get back on track if we follow the frame pointer chain (or whatever is appropriate
    // on this ABI) so we allow one invalid frame to be in the stack.  Ideally we'll mark this frame specially at some
    // point and indicate to the user that the unwinder had a hiccup.  Often when this happens we will miss a frame of
    // the program's actual stack in the unwind and we want to flag that for the user somehow.
    bool
    IsSkipFrame () const;


    //------------------------------------------------------------------
    /// Determines if a SymbolContext is a trap handler or not
    ///
    /// Given a SymbolContext, determines if this is a trap handler function
    /// aka asynchronous signal handler.
    ///
    /// @return
    ///     Returns true if the SymbolContext is a trap handler.
    //------------------------------------------------------------------
    bool
    IsTrapHandlerSymbol (lldb_private::Process *process, const lldb_private::SymbolContext &m_sym_ctx) const;

    // Provide a location for where THIS function saved the CALLER's register value
    // Or a frame "below" this one saved it, i.e. a function called by this one, preserved a register that this
    // function didn't modify/use.
    //
    // The RegisterLocation type may be set to eRegisterNotAvailable -- this will happen for a volatile register
    // being queried mid-stack.  Instead of floating frame 0's contents of that register up the stack (which may
    // or may not be the value of that reg when the function was executing), we won't return any value.
    //
    // If a non-volatile register (a "preserved" register) is requested mid-stack and no frames "below" the requested
    // stack have saved the register anywhere, it is safe to assume that frame 0's register values are still the same
    // as the requesting frame's.
    lldb_private::UnwindLLDB::RegisterSearchResult
    SavedLocationForRegister (uint32_t lldb_regnum, lldb_private::UnwindLLDB::RegisterLocation &regloc);

    bool
    ReadRegisterValueFromRegisterLocation (lldb_private::UnwindLLDB::RegisterLocation regloc,
                                           const lldb_private::RegisterInfo *reg_info,
                                           lldb_private::RegisterValue &value);

    bool
    WriteRegisterValueToRegisterLocation (lldb_private::UnwindLLDB::RegisterLocation regloc,
                                          const lldb_private::RegisterInfo *reg_info,
                                          const lldb_private::RegisterValue &value);

    //------------------------------------------------------------------
    /// If the unwind has to the caller frame has failed, try something else
    ///
    /// If lldb is using an assembly language based UnwindPlan for a frame and
    /// the unwind to the caller frame fails, try falling back to a generic
    /// UnwindPlan (architecture default unwindplan) to see if that might work
    /// better.  This is mostly helping to work around problems where the 
    /// assembly language inspection fails on hand-written assembly code.
    ///
    /// @return
    ///     Returns true if a fallback unwindplan was found & was installed.
    //------------------------------------------------------------------
    bool
    TryFallbackUnwindPlan ();

    // Get the contents of a general purpose (address-size) register for this frame
    // (usually retrieved from the next frame)
    bool
    ReadGPRValue (lldb::RegisterKind register_kind, uint32_t regnum, lldb::addr_t &value);

    lldb::UnwindPlanSP
    GetFastUnwindPlanForFrame ();

    lldb::UnwindPlanSP
    GetFullUnwindPlanForFrame ();

    void
    UnwindLogMsg (const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

    void
    UnwindLogMsgVerbose (const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

    bool
    IsUnwindPlanValidForCurrentPC(lldb::UnwindPlanSP unwind_plan_sp, int &valid_pc_offset);


    lldb_private::Thread& m_thread;

    ///
    // The following tell us how to retrieve the CALLER's register values (ie the "previous" frame, aka the frame above)
    // i.e. where THIS frame saved them
    ///

    lldb::UnwindPlanSP m_fast_unwind_plan_sp;     // may be NULL
    lldb::UnwindPlanSP m_full_unwind_plan_sp;
    lldb::UnwindPlanSP m_fallback_unwind_plan_sp; // may be NULL

    bool m_all_registers_available;               // Can we retrieve all regs or just nonvolatile regs?
    int m_frame_type;                             // enum FrameType

    lldb::addr_t m_cfa;
    lldb_private::Address m_start_pc;
    lldb_private::Address m_current_pc;

    int m_current_offset;                         // how far into the function we've executed; -1 if unknown
                                                  // 0 if no instructions have been executed yet.

    int m_current_offset_backed_up_one;           // how far into the function we've executed; -1 if unknown
                                                  // 0 if no instructions have been executed yet.
                                                  // On architectures where the return address on the stack points
                                                  // to the instruction after the CALL, this value will have 1
                                                  // subtracted from it.  Else a function that ends in a CALL will
                                                  // have an offset pointing into the next function's address range.
                                                  // m_current_pc has the actual address of the "current" pc.

    lldb_private::SymbolContext& m_sym_ctx;
    bool m_sym_ctx_valid;                         // if ResolveSymbolContextForAddress fails, don't try to use m_sym_ctx

    uint32_t m_frame_number;                      // What stack frame this RegisterContext is

    std::map<uint32_t, lldb_private::UnwindLLDB::RegisterLocation> m_registers; // where to find reg values for this frame

    lldb_private::UnwindLLDB& m_parent_unwind;    // The UnwindLLDB that is creating this RegisterContextLLDB

    //------------------------------------------------------------------
    // For RegisterContextLLDB only
    //------------------------------------------------------------------

    DISALLOW_COPY_AND_ASSIGN (RegisterContextLLDB);
};

} // namespace lldb_private

#endif  // lldb_RegisterContextLLDB_h_
