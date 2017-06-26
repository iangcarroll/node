// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_

#include <array>
#include "src/ast/ast.h"
#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecode-source-info.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/interpreter/handler-table-builder.h"
#include "src/source-position-table.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;

namespace interpreter {

class BytecodeLabel;
class BytecodeNode;
class BytecodeRegisterOptimizer;
class BytecodeJumpTable;
class Register;

class V8_EXPORT_PRIVATE BytecodeArrayBuilder final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  BytecodeArrayBuilder(
      Isolate* isolate, Zone* zone, int parameter_count, int locals_count,
      FunctionLiteral* literal = nullptr,
      SourcePositionTableBuilder::RecordingMode source_position_mode =
          SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS);

  Handle<BytecodeArray> ToBytecodeArray(Isolate* isolate);

  // Get the number of parameters expected by function.
  int parameter_count() const {
    DCHECK_GE(parameter_count_, 0);
    return parameter_count_;
  }

  // Get the number of locals required for bytecode array.
  int locals_count() const {
    DCHECK_GE(local_register_count_, 0);
    return local_register_count_;
  }

  // Returns the number of fixed (non-temporary) registers.
  int fixed_register_count() const { return locals_count(); }

  // Returns the number of fixed and temporary registers.
  int total_register_count() const {
    DCHECK_LE(fixed_register_count(),
              register_allocator()->maximum_register_count());
    return register_allocator()->maximum_register_count();
  }

  Register Local(int index) const;
  Register Parameter(int parameter_index) const;
  Register Receiver() const;

  // Constant loads to accumulator.
  BytecodeArrayBuilder& LoadConstantPoolEntry(size_t entry);
  BytecodeArrayBuilder& LoadLiteral(v8::internal::Smi* value);
  BytecodeArrayBuilder& LoadLiteral(const AstRawString* raw_string);
  BytecodeArrayBuilder& LoadLiteral(const Scope* scope);
  BytecodeArrayBuilder& LoadLiteral(const AstValue* ast_value);
  BytecodeArrayBuilder& LoadUndefined();
  BytecodeArrayBuilder& LoadNull();
  BytecodeArrayBuilder& LoadTheHole();
  BytecodeArrayBuilder& LoadTrue();
  BytecodeArrayBuilder& LoadFalse();

  // Global loads to the accumulator and stores from the accumulator.
  BytecodeArrayBuilder& LoadGlobal(const AstRawString* name, int feedback_slot,
                                   TypeofMode typeof_mode);
  BytecodeArrayBuilder& StoreGlobal(const AstRawString* name, int feedback_slot,
                                    LanguageMode language_mode);

  // Load the object at |slot_index| at |depth| in the context chain starting
  // with |context| into the accumulator.
  enum ContextSlotMutability { kImmutableSlot, kMutableSlot };
  BytecodeArrayBuilder& LoadContextSlot(Register context, int slot_index,
                                        int depth,
                                        ContextSlotMutability immutable);

  // Stores the object in the accumulator into |slot_index| at |depth| in the
  // context chain starting with |context|.
  BytecodeArrayBuilder& StoreContextSlot(Register context, int slot_index,
                                         int depth);

  // Load from a module variable into the accumulator. |depth| is the depth of
  // the current context relative to the module context.
  BytecodeArrayBuilder& LoadModuleVariable(int cell_index, int depth);

  // Store from the accumulator into a module variable. |depth| is the depth of
  // the current context relative to the module context.
  BytecodeArrayBuilder& StoreModuleVariable(int cell_index, int depth);

  // Register-accumulator transfers.
  BytecodeArrayBuilder& LoadAccumulatorWithRegister(Register reg);
  BytecodeArrayBuilder& StoreAccumulatorInRegister(Register reg);

  // Register-register transfer.
  BytecodeArrayBuilder& MoveRegister(Register from, Register to);

  // Named load property.
  BytecodeArrayBuilder& LoadNamedProperty(Register object,
                                          const AstRawString* name,
                                          int feedback_slot);
  // Keyed load property. The key should be in the accumulator.
  BytecodeArrayBuilder& LoadKeyedProperty(Register object, int feedback_slot);
  // Named load property of the @@iterator symbol.
  BytecodeArrayBuilder& LoadIteratorProperty(Register object,
                                             int feedback_slot);
  // Named load property of the @@asyncIterator symbol.
  BytecodeArrayBuilder& LoadAsyncIteratorProperty(Register object,
                                                  int feedback_slot);

  // Store properties. Flag for NeedsSetFunctionName() should
  // be in the accumulator.
  BytecodeArrayBuilder& StoreDataPropertyInLiteral(
      Register object, Register name, DataPropertyInLiteralFlags flags,
      int feedback_slot);

  // Collect type information for developer tools. The value for which we
  // record the type is stored in the accumulator.
  BytecodeArrayBuilder& CollectTypeProfile(int position);

  // Store a property named by a property name. The value to be stored should be
  // in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object,
                                           const AstRawString* name,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  // Store a property named by a constant from the constant pool. The value to
  // be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object,
                                           size_t constant_pool_entry,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  // Store an own property named by a constant from the constant pool. The
  // value to be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedOwnProperty(Register object,
                                              const AstRawString* name,
                                              int feedback_slot);
  // Store a property keyed by a value in a register. The value to be stored
  // should be in the accumulator.
  BytecodeArrayBuilder& StoreKeyedProperty(Register object, Register key,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  // Store the home object property. The value to be stored should be in the
  // accumulator.
  BytecodeArrayBuilder& StoreHomeObjectProperty(Register object,
                                                int feedback_slot,
                                                LanguageMode language_mode);

  // Lookup the variable with |name|.
  BytecodeArrayBuilder& LoadLookupSlot(const AstRawString* name,
                                       TypeofMode typeof_mode);

  // Lookup the variable with |name|, which is known to be at |slot_index| at
  // |depth| in the context chain if not shadowed by a context extension
  // somewhere in that context chain.
  BytecodeArrayBuilder& LoadLookupContextSlot(const AstRawString* name,
                                              TypeofMode typeof_mode,
                                              int slot_index, int depth);

  // Lookup the variable with |name|, which has its feedback in |feedback_slot|
  // and is known to be global if not shadowed by a context extension somewhere
  // up to |depth| in that context chain.
  BytecodeArrayBuilder& LoadLookupGlobalSlot(const AstRawString* name,
                                             TypeofMode typeof_mode,
                                             int feedback_slot, int depth);

  // Store value in the accumulator into the variable with |name|.
  BytecodeArrayBuilder& StoreLookupSlot(
      const AstRawString* name, LanguageMode language_mode,
      LookupHoistingMode lookup_hoisting_mode);

  // Create a new closure for a SharedFunctionInfo which will be inserted at
  // constant pool index |shared_function_info_entry|.
  BytecodeArrayBuilder& CreateClosure(size_t shared_function_info_entry,
                                      int slot, int flags);

  // Create a new local context for a |scope| and a closure which should be
  // in the accumulator.
  BytecodeArrayBuilder& CreateBlockContext(const Scope* scope);

  // Create a new context for a catch block with |exception|, |name|,
  // |scope|, and the closure in the accumulator.
  BytecodeArrayBuilder& CreateCatchContext(Register exception,
                                           const AstRawString* name,
                                           const Scope* scope);

  // Create a new context with size |slots|.
  BytecodeArrayBuilder& CreateFunctionContext(int slots);

  // Create a new eval context with size |slots|.
  BytecodeArrayBuilder& CreateEvalContext(int slots);

  // Creates a new context with the given |scope| for a with-statement
  // with the |object| in a register and the closure in the accumulator.
  BytecodeArrayBuilder& CreateWithContext(Register object, const Scope* scope);

  // Create a new arguments object in the accumulator.
  BytecodeArrayBuilder& CreateArguments(CreateArgumentsType type);

  // Literals creation.  Constant elements should be in the accumulator.
  BytecodeArrayBuilder& CreateRegExpLiteral(const AstRawString* pattern,
                                            int literal_index, int flags);
  BytecodeArrayBuilder& CreateArrayLiteral(size_t constant_elements_entry,
                                           int literal_index, int flags);
  BytecodeArrayBuilder& CreateObjectLiteral(size_t constant_properties_entry,
                                            int literal_index, int flags,
                                            Register output);

  // Push the context in accumulator as the new context, and store in register
  // |context|.
  BytecodeArrayBuilder& PushContext(Register context);

  // Pop the current context and replace with |context|.
  BytecodeArrayBuilder& PopContext(Register context);

  // Call a JS function which is known to be a property of a JS object. The
  // JSFunction or Callable to be called should be in |callable|. The arguments
  // should be in |args|, with the receiver in |args[0]|. The call type of the
  // expression is in |call_type|. Type feedback is recorded in the
  // |feedback_slot| in the type feedback vector.
  BytecodeArrayBuilder& CallProperty(Register callable, RegisterList args,
                                     int feedback_slot);

  // Call a JS function with an known undefined receiver. The JSFunction or
  // Callable to be called should be in |callable|. The arguments should be in
  // |args|, with no receiver as it is implicitly set to undefined. Type
  // feedback is recorded in the |feedback_slot| in the type feedback vector.
  BytecodeArrayBuilder& CallUndefinedReceiver(Register callable,
                                              RegisterList args,
                                              int feedback_slot);

  // Call a JS function with an any receiver, possibly (but not necessarily)
  // undefined. The JSFunction or Callable to be called should be in |callable|.
  // The arguments should be in |args|, with the receiver in |args[0]|. Type
  // feedback is recorded in the |feedback_slot| in the type feedback vector.
  BytecodeArrayBuilder& CallAnyReceiver(Register callable, RegisterList args,
                                        int feedback_slot);

  // Tail call into a JS function. The JSFunction or Callable to be called
  // should be in |callable|. The arguments should be in |args|, with the
  // receiver in |args[0]|. Type feedback is recorded in the |feedback_slot| in
  // the type feedback vector.
  BytecodeArrayBuilder& TailCall(Register callable, RegisterList args,
                                 int feedback_slot);

  // Call a JS function. The JSFunction or Callable to be called should be in
  // |callable|, the receiver in |args[0]| and the arguments in |args[1]|
  // onwards. The final argument must be a spread.
  BytecodeArrayBuilder& CallWithSpread(Register callable, RegisterList args);

  // Call the Construct operator. The accumulator holds the |new_target|.
  // The |constructor| is in a register and arguments are in |args|.
  BytecodeArrayBuilder& Construct(Register constructor, RegisterList args,
                                  int feedback_slot);

  // Call the Construct operator for use with a spread. The accumulator holds
  // the |new_target|. The |constructor| is in a register and arguments are in
  // |args|. The final argument must be a spread.
  BytecodeArrayBuilder& ConstructWithSpread(Register constructor,
                                            RegisterList args);

  // Call the runtime function with |function_id| and arguments |args|.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id,
                                    RegisterList args);
  // Call the runtime function with |function_id| with single argument |arg|.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id,
                                    Register arg);
  // Call the runtime function with |function_id| with no arguments.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id);

  // Call the runtime function with |function_id| and arguments |args|, that
  // returns a pair of values. The return values will be returned in
  // |return_pair|.
  BytecodeArrayBuilder& CallRuntimeForPair(Runtime::FunctionId function_id,
                                           RegisterList args,
                                           RegisterList return_pair);
  // Call the runtime function with |function_id| with single argument |arg|
  // that returns a pair of values. The return values will be returned in
  // |return_pair|.
  BytecodeArrayBuilder& CallRuntimeForPair(Runtime::FunctionId function_id,
                                           Register arg,
                                           RegisterList return_pair);

  // Call the JS runtime function with |context_index| and arguments |args|.
  BytecodeArrayBuilder& CallJSRuntime(int context_index, RegisterList args);

  // Operators (register holds the lhs value, accumulator holds the rhs value).
  // Type feedback will be recorded in the |feedback_slot|
  BytecodeArrayBuilder& BinaryOperation(Token::Value binop, Register reg,
                                        int feedback_slot);
  BytecodeArrayBuilder& BinaryOperationSmiLiteral(Token::Value binop,
                                                  Smi* literal,
                                                  int feedback_slot);

  // Count Operators (value stored in accumulator).
  // Type feedback will be recorded in the |feedback_slot|
  BytecodeArrayBuilder& CountOperation(Token::Value op, int feedback_slot);

  enum class ToBooleanMode {
    kConvertToBoolean,  // Perform ToBoolean conversion on accumulator.
    kAlreadyBoolean,    // Accumulator is already a Boolean.
  };

  // Unary Operators.
  BytecodeArrayBuilder& LogicalNot(ToBooleanMode mode);
  BytecodeArrayBuilder& TypeOf();

  // Expects a heap object in the accumulator. Returns its super constructor in
  // the register |out| if it passes the IsConstructor test. Otherwise, it
  // throws a TypeError exception.
  BytecodeArrayBuilder& GetSuperConstructor(Register out);

  // Deletes property from an object. This expects that accumulator contains
  // the key to be deleted and the register contains a reference to the object.
  BytecodeArrayBuilder& Delete(Register object, LanguageMode language_mode);

  // Tests.
  BytecodeArrayBuilder& CompareOperation(Token::Value op, Register reg,
                                         int feedback_slot);
  BytecodeArrayBuilder& CompareOperation(Token::Value op, Register reg);
  BytecodeArrayBuilder& CompareUndetectable();
  BytecodeArrayBuilder& CompareUndefined();
  BytecodeArrayBuilder& CompareNull();
  BytecodeArrayBuilder& CompareNil(Token::Value op, NilValue nil);
  BytecodeArrayBuilder& CompareTypeOf(
      TestTypeOfFlags::LiteralFlag literal_flag);

  // Converts accumulator and stores result in register |out|.
  BytecodeArrayBuilder& ToObject(Register out);
  BytecodeArrayBuilder& ToName(Register out);
  BytecodeArrayBuilder& ToNumber(Register out, int feedback_slot);

  // Converts accumulator to a primitive and then to a string, and stores result
  // in register |out|.
  BytecodeArrayBuilder& ToPrimitiveToString(Register out, int feedback_slot);
  // Concatenate all the string values in |operand_registers| into a string
  // and store result in the accumulator.
  BytecodeArrayBuilder& StringConcat(RegisterList operand_registers);

  // Flow Control.
  BytecodeArrayBuilder& Bind(BytecodeLabel* label);
  BytecodeArrayBuilder& Bind(const BytecodeLabel& target, BytecodeLabel* label);
  BytecodeArrayBuilder& Bind(BytecodeJumpTable* jump_table, int case_value);

  BytecodeArrayBuilder& Jump(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpLoop(BytecodeLabel* label, int loop_depth);

  BytecodeArrayBuilder& JumpIfTrue(ToBooleanMode mode, BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfFalse(ToBooleanMode mode, BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNotHole(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfJSReceiver(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNull(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNotNull(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfUndefined(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNotUndefined(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNil(BytecodeLabel* label, Token::Value op,
                                  NilValue nil);
  BytecodeArrayBuilder& JumpIfNotNil(BytecodeLabel* label, Token::Value op,
                                     NilValue nil);

  BytecodeArrayBuilder& SwitchOnSmiNoFeedback(BytecodeJumpTable* jump_table);

  BytecodeArrayBuilder& StackCheck(int position);

  // Sets the pending message to the value in the accumulator, and returns the
  // previous pending message in the accumulator.
  BytecodeArrayBuilder& SetPendingMessage();

  BytecodeArrayBuilder& Throw();
  BytecodeArrayBuilder& ReThrow();
  BytecodeArrayBuilder& Return();
  BytecodeArrayBuilder& ThrowReferenceErrorIfHole(const AstRawString* name);
  BytecodeArrayBuilder& ThrowSuperNotCalledIfHole();
  BytecodeArrayBuilder& ThrowSuperAlreadyCalledIfNotHole();

  // Debugger.
  BytecodeArrayBuilder& Debugger();

  // Increment the block counter at the given slot (block code coverage).
  BytecodeArrayBuilder& IncBlockCounter(int slot);

  // Complex flow control.
  BytecodeArrayBuilder& ForInPrepare(Register receiver,
                                     RegisterList cache_info_triple);
  BytecodeArrayBuilder& ForInContinue(Register index, Register cache_length);
  BytecodeArrayBuilder& ForInNext(Register receiver, Register index,
                                  RegisterList cache_type_array_pair,
                                  int feedback_slot);
  BytecodeArrayBuilder& ForInStep(Register index);

  // Generators.
  BytecodeArrayBuilder& SuspendGenerator(Register generator,
                                         RegisterList registers,
                                         SuspendFlags flags);
  BytecodeArrayBuilder& RestoreGeneratorState(Register generator);
  BytecodeArrayBuilder& RestoreGeneratorRegisters(Register generator,
                                                  RegisterList registers);

  // Exception handling.
  BytecodeArrayBuilder& MarkHandler(int handler_id,
                                    HandlerTable::CatchPrediction will_catch);
  BytecodeArrayBuilder& MarkTryBegin(int handler_id, Register context);
  BytecodeArrayBuilder& MarkTryEnd(int handler_id);

  // Creates a new handler table entry and returns a {hander_id} identifying the
  // entry, so that it can be referenced by above exception handling support.
  int NewHandlerEntry() { return handler_table_builder()->NewHandlerEntry(); }

  // Allocates a new jump table of given |size| and |case_value_base| in the
  // constant pool.
  BytecodeJumpTable* AllocateJumpTable(int size, int case_value_base);

  // Gets a constant pool entry.
  size_t GetConstantPoolEntry(const AstRawString* raw_string);
  size_t GetConstantPoolEntry(const AstValue* heap_number);
  size_t GetConstantPoolEntry(const Scope* scope);
#define ENTRY_GETTER(NAME, ...) size_t NAME##ConstantPoolEntry();
  SINGLETON_CONSTANT_ENTRY_TYPES(ENTRY_GETTER)
#undef ENTRY_GETTER

  // Allocates a slot in the constant pool which can later be set.
  size_t AllocateDeferredConstantPoolEntry();
  // Sets the deferred value into an allocated constant pool entry.
  void SetDeferredConstantPoolEntry(size_t entry, Handle<Object> object);

  void InitializeReturnPosition(FunctionLiteral* literal);

  void SetStatementPosition(Statement* stmt) {
    if (stmt->position() == kNoSourcePosition) return;
    latest_source_info_.MakeStatementPosition(stmt->position());
  }

  void SetExpressionPosition(Expression* expr) {
    if (expr->position() == kNoSourcePosition) return;
    if (!latest_source_info_.is_statement()) {
      // Ensure the current expression position is overwritten with the
      // latest value.
      latest_source_info_.MakeExpressionPosition(expr->position());
    }
  }

  void SetExpressionAsStatementPosition(Expression* expr) {
    if (expr->position() == kNoSourcePosition) return;
    latest_source_info_.MakeStatementPosition(expr->position());
  }

  bool RequiresImplicitReturn() const { return !exit_seen_in_block_; }

  // Returns the raw operand value for the given register or register list.
  uint32_t GetInputRegisterOperand(Register reg);
  uint32_t GetOutputRegisterOperand(Register reg);
  uint32_t GetInputRegisterListOperand(RegisterList reg_list);
  uint32_t GetOutputRegisterListOperand(RegisterList reg_list);

  // Outputs raw register transfer bytecodes without going through the register
  // optimizer.
  void OutputLdarRaw(Register reg);
  void OutputStarRaw(Register reg);
  void OutputMovRaw(Register src, Register dest);

  // Accessors
  BytecodeRegisterAllocator* register_allocator() {
    return &register_allocator_;
  }
  const BytecodeRegisterAllocator* register_allocator() const {
    return &register_allocator_;
  }
  Zone* zone() const { return zone_; }

 private:
  // Maximum sized packed bytecode is comprised of a prefix bytecode,
  // plus the actual bytecode, plus the maximum number of operands times
  // the maximum operand size.
  static const size_t kMaxSizeOfPackedBytecode =
      2 * sizeof(Bytecode) +
      Bytecodes::kMaxOperands * static_cast<size_t>(OperandSize::kLast);

  // Constants that act as placeholders for jump operands to be
  // patched. These have operand sizes that match the sizes of
  // reserved constant pool entries.
  static const uint32_t k8BitJumpPlaceholder = 0x7f;
  static const uint32_t k16BitJumpPlaceholder =
      k8BitJumpPlaceholder | (k8BitJumpPlaceholder << 8);
  static const uint32_t k32BitJumpPlaceholder =
      k16BitJumpPlaceholder | (k16BitJumpPlaceholder << 16);

  void PatchJump(size_t jump_target, size_t jump_location);
  void PatchJumpWith8BitOperand(size_t jump_location, int delta);
  void PatchJumpWith16BitOperand(size_t jump_location, int delta);
  void PatchJumpWith32BitOperand(size_t jump_location, int delta);

  // Emit a non-jump bytecode with the given integer operand values.
  template <AccumulatorUse accumulator_use, OperandType... operand_types>
  void Write(Bytecode bytecode, BytecodeSourceInfo source_info,
             std::array<uint32_t, sizeof...(operand_types)> operand_values);

  // Emit a jump bytecode with the given integer operand values.
  template <AccumulatorUse accumulator_use, OperandType... operand_types>
  void WriteJump(Bytecode bytecode, BytecodeSourceInfo source_info,
                 BytecodeLabel* label,
                 std::array<uint32_t, sizeof...(operand_types)> operand_values);

  // Emit a switch bytecode with the given integer operand values.
  template <AccumulatorUse accumulator_use, OperandType... operand_types>
  void WriteSwitch(
      Bytecode bytecode, BytecodeSourceInfo source_info,
      BytecodeJumpTable* jump_table,
      std::array<uint32_t, sizeof...(operand_types)> operand_values);

  // Emit the actual bytes of a bytecode and its operands. Called by Emit for
  // jump and non-jump bytecodes.
  template <OperandType... operand_types>
  void EmitBytecode(
      Bytecode bytecode,
      std::array<uint32_t, sizeof...(operand_types)> operand_values);

#define DECLARE_BYTECODE_OUTPUT(Name, ...)                               \
  template <typename... Operands>                                        \
  INLINE(void Output##Name(Operands... operands));                       \
                                                                         \
  template <typename... Operands>                                        \
  INLINE(void Output##Name(BytecodeLabel* label, Operands... operands)); \
                                                                         \
  template <typename... Operands>                                        \
  INLINE(                                                                \
      void Output##Name(BytecodeJumpTable* jump_table, Operands... operands));

  BYTECODE_LIST(DECLARE_BYTECODE_OUTPUT)
#undef DECLARE_OPERAND_TYPE_INFO

  INLINE(void OutputSwitchOnSmiNoFeedback(BytecodeJumpTable* jump_table));

  void MaybeElideLastBytecode(Bytecode next_bytecode, bool has_source_info);
  void InvalidateLastBytecode();

  bool RegisterIsValid(Register reg) const;
  bool RegisterListIsValid(RegisterList reg_list) const;

  // Set position for return.
  void SetReturnPosition();

  // Returns the current source position for the given |bytecode|.
  INLINE(BytecodeSourceInfo CurrentSourcePosition(Bytecode bytecode));

  // Update the source table for the current offset with the given source info.
  void AttachSourceInfo(const BytecodeSourceInfo& source_info);
  // Sets a deferred source info which should be emitted before any future
  // source info (either attached to a following bytecode or as a nop).
  void SetDeferredSourceInfo(BytecodeSourceInfo source_info);
  // Attach the deferred and given source infos to the current bytecode,
  // possibly emitting a nop for the deferred info if both the deferred and
  // given source infos are valid.
  void AttachDeferredAndCurrentSourceInfo(BytecodeSourceInfo source_info);

  // Not implemented as the illegal bytecode is used inside internally
  // to indicate a bytecode field is not valid or an error has occured
  // during bytecode generation.
  BytecodeArrayBuilder& Illegal();

  template <Bytecode bytecode, AccumulatorUse accumulator_use,
            OperandType... operand_types>
  void PrepareToOutputBytecode();

  void LeaveBasicBlock() { exit_seen_in_block_ = false; }

  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }
  const ZoneVector<uint8_t>* bytecodes() const { return &bytecodes_; }
  ConstantArrayBuilder* constant_array_builder() {
    return &constant_array_builder_;
  }
  const ConstantArrayBuilder* constant_array_builder() const {
    return &constant_array_builder_;
  }
  HandlerTableBuilder* handler_table_builder() {
    return &handler_table_builder_;
  }
  SourcePositionTableBuilder* source_position_table_builder() {
    return &source_position_table_builder_;
  }
  const FeedbackVectorSpec* feedback_vector_spec() const {
    return literal_->feedback_vector_spec();
  }
  void set_latest_source_info(BytecodeSourceInfo source_info) {
    latest_source_info_ = source_info;
  }

  Zone* zone_;
  ZoneVector<uint8_t> bytecodes_;
  FunctionLiteral* literal_;

  ConstantArrayBuilder constant_array_builder_;
  HandlerTableBuilder handler_table_builder_;
  SourcePositionTableBuilder source_position_table_builder_;

  BytecodeRegisterAllocator register_allocator_;
  BytecodeRegisterOptimizer* register_optimizer_;
  BytecodeSourceInfo latest_source_info_;
  BytecodeSourceInfo deferred_source_info_;

  int parameter_count_;
  int local_register_count_;
  int return_position_;
  int unbound_jumps_;

  bool bytecode_generated_;
  bool elide_noneffectful_bytecodes_;
  bool exit_seen_in_block_;
  bool last_bytecode_had_source_info_;

  size_t last_bytecode_offset_;
  Bytecode last_bytecode_;

  static int const kNoFeedbackSlot = 0;

  friend class BytecodeArrayBuilderTest;
  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayBuilder);
};

V8_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os, const BytecodeArrayBuilder::ToBooleanMode& mode);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
