#include "stdafx.h"
#include "foo_driver.h"
#include "cmd_system.h"

#define FOO_TOKENIZE_DBG(X)
//#define FOO_TOKENIZE_DBG(X)  fcs.LOG_DEBUG() << X << FooBase::Log::FooLogEnd

using namespace FooBase;
using namespace FooScript;
using Parser::FooDriver;

FooDriver::FooDriver(FooCmdSystem& Fcs) :
fcs(Fcs), parser(*this), vm(Fcs) {
    Init();
}

void FooDriver::Execute(const tCmdString& CmdString) {
    Parse(CmdString);
}

void FooDriver::Reset() {
    // �����Դ
    vm.Reset();
    while (exp_value_stack.empty() == false) {
        exp_value_stack.pop();
    }
    while (static_class_stack.empty() == false) {
        static_class_stack.pop();
    }
    // ���³�ʼ��
    Init();
}

void FooDriver::PumpElemID(const tToken& Token) {
    FOO_TOKENIZE_DBG(Token);
    FooVM::tSClassSPtr ParentpClosure = static_class_stack.top();
    auto Index = ParentpClosure->GetLocalIndex(Token);
    exp_value_stack.push(Index);
}

void FooDriver::PumpLiterString(const std::string& v) {
    FOO_TOKENIZE_DBG(v);
    auto Index = vm.GetConstIndex(v);
    exp_value_stack.push(Index);
}

void FooDriver::PumpLiterInt(FooScript::Type::tInt v) {
    FOO_TOKENIZE_DBG(v);
    auto Index = vm.GetConstIndex(v);
    exp_value_stack.push(Index);
}

void FooDriver::PumpLiterFloat(double v) {
    FOO_TOKENIZE_DBG(v);
    auto Index = vm.GetConstIndex(v);
    exp_value_stack.push(Index);
}

void FooDriver::PumpStateAssign() {
    // ��ֵ���ʽ���������Ԫ���ʽ��
    // ����ֵͬʱΪ����ֵ
    FOO_TOKENIZE_DBG("...=...");
    //VMPushTwoArgInstr(Parser::eInstr::I_ASSIGN);
    //DealTwoArgExp(Parser::eInstr::I_ASSIGN);
    vm.PushInstr(Parser::eInstr::I_ASSIGN);
    auto RightIndex = exp_value_stack.top();
    TransValueExp();
    TransValueExp();
    exp_value_stack.push(RightIndex);
}

void FooDriver::PumpStateClassDefStart() {
    FOO_TOKENIZE_DBG("(");
}

void FooDriver::PumpStateClassDefEnd() {
    FOO_TOKENIZE_DBG("):");
}

void FooDriver::PumpStateClass(const tToken& Token) {
    FOO_TOKENIZE_DBG("class...");
    auto Parent = static_class_stack.top();
    tIndex SClassIndex = 0;
    // ������̬����󣬷��ؾ�̬�������е�����
    auto CurClass = vm.CreateSClass(Token, Parent, SClassIndex);
    static_class_stack.push(CurClass);
    // ѹ��ָ��
    // ѹ�����ǰ���Ծֲ�����������ִ���������е�����
    VMPushOneArgInstr(Parser::eInstr::I_CLASS);
    // ѹ�뾲̬������������󴴽�ʱ����Ҫ�����ľ�̬��Ϊ����
    VMPushData(SClassIndex);
}

void FooDriver::PumpStateClassEnd() {
    FOO_TOKENIZE_DBG("#");
    // ����ֻѹ������
    // ��Ϊ���ִ�н�������ڹ̶��Ĵ�����
    VMPushInstr(Parser::eInstr::I_CLASS_END);
    static_class_stack.top()->SetEndCursor(vm.GetCurrentInstrCount());
    static_class_stack.pop();
}

void FooDriver::PumpStateCompleteCallbackStart() {
    FOO_TOKENIZE_DBG("callback...");
    auto Parent = static_class_stack.top();
    tIndex SClassIndex = 0;
    // ������̬����󣬷��ؾ�̬�������е�����
    auto CurClass = vm.CreateSClass("", Parent, SClassIndex);
    static_class_stack.push(CurClass);
    // ѹ��ָ��
    VMPushInstr(Parser::eInstr::I_CLASS_COMPLETE_CALLBACK);
    // ѹ�뾲̬������������󴴽�ʱ����Ҫ�����ľ�̬��Ϊ����
    VMPushData(SClassIndex);
}

void FooDriver::PumpStateCompleteCallbackEnd() {
    // ��ɻص�������Ҳ��һ���࣬����������ʱ���߼�����ͨ��һ��
    PumpStateClassEnd();
}

void FooDriver::PumpStateClassCallStart() {
    FOO_TOKENIZE_DBG("(");
    //fun_call_stack.push(exp_value_stack.top());
    VMPushOneArgInstr(Parser::eInstr::I_CLASS_CALL_START);
}

void FooDriver::PumpStateClassCallEnd() {
    FOO_TOKENIZE_DBG(")");
    VMPushInstr(Parser::eInstr::I_CLASS_CALL_END);
    auto RetIndex = FlagValIndexRegRet;
    exp_value_stack.push(RetIndex);
    //VMPushRet(RetIndex);
    //fun_call_stack.pop();
}

void FooDriver::PumpStateClassCallWithoutArg() {
    FOO_TOKENIZE_DBG("()");
    //fun_call_stack.push(exp_value_stack.top());
    VMPushOneArgInstr(Parser::eInstr::I_CLASS_CALL_WITHOUT_ARG);
    auto RetIndex = FlagValIndexRegRet;
    exp_value_stack.push(RetIndex);
    //VMPushRet(RetIndex);
}

void FooDriver::PumpClassArguClause() {
    FOO_TOKENIZE_DBG(",");
    VMPushOneArgInstr(Parser::eInstr::I_CLASS_ARGU_CLAUSE);
}

void FooDriver::PumpClassParaClause() {
    FOO_TOKENIZE_DBG(",");
    // �ඨ��Ĳ����Ӿ������Ĭ�ϲ�����ֱ����ջ����
    exp_value_stack.pop();
    static_class_stack.top()->IncParamCount();
}

void FooDriver::PumpClassParaSetClause() {
    FOO_TOKENIZE_DBG("...=...,");
    static_class_stack.top()->IncParamCount();
    VMPushTwoArgInstr(Parser::eInstr::I_CLASS_SET_PARA_CLAUSE);
}

void FooDriver::PumpExpMember(const tToken& Token) {
    FOO_TOKENIZE_DBG("." + Token);
    // ��������ѹջ����ջ������ֱ��ѹ��ָ��������
    auto right = vm.GetConstIndex(Token);

    auto left = exp_value_stack.top();
    if (vm.IsMemberIndex(left)) {
        // �����֮ǰջ���ǳ�Ա����
        // ���˳������Ա����������
        exp_value_stack.pop();
        // ѹ���µ��ӳ�Ա
        exp_value_stack.push(right);
        // �����ӳ�Ա����
        auto NewIndex = vm.IncMemberIndex(left);
        // ѹ�ر��ʽջ��
        exp_value_stack.push(NewIndex);
    }
    else {
        auto MemIndex = vm.GetMemberIndex(false);
        exp_value_stack.push(right);
        exp_value_stack.push(MemIndex);
    }
}   

void FooDriver::PumpExpContextMember(const tToken& Token) {
    FOO_TOKENIZE_DBG("." + Token);
    auto right = vm.GetConstIndex(Token);
    auto MemIndex = vm.GetMemberIndex(true);
    exp_value_stack.push(right);
    exp_value_stack.push(MemIndex);
}

void FooDriver::PumpExpContextStaticMember(const tToken& Token) {

}

void FooDriver::PumpExpAdd() {
    FOO_TOKENIZE_DBG("+");
    DealTwoArgExp(Parser::eInstr::I_ADD);
}

void FooDriver::PumpExpMul() {
    FOO_TOKENIZE_DBG("*");
    DealTwoArgExp(Parser::eInstr::I_MUL);
}

void FooDriver::PumpExpMin() {
    FOO_TOKENIZE_DBG("-");
    DealTwoArgExp(Parser::eInstr::I_MIN);
}

void FooDriver::PumpExpMod() {
    FOO_TOKENIZE_DBG("%");
    DealTwoArgExp(Parser::eInstr::I_MOD);
}

void FooDriver::PumpExpDiv() {
    FOO_TOKENIZE_DBG("/");
    DealTwoArgExp(Parser::eInstr::I_DIV);
}

void FooDriver::ExitFooMode() {
    fcs.ExitFooMode();
}

void FooDriver::ListStream() const {
    vm.ListStream();
}

void FooDriver::EnterMutualMode() {
    vm.EnterMutualMode();
}

void FooDriver::DebugExecuteByStepInto() {
    vm.DebugExecuteByStepInto();
}

void FooDriver::Init() {
    parser.set_debug_level(false);

    // ����һ����������
    static_class_stack.push(vm.GetTopSClass());
}

void FooDriver::DealOneArgExp(Parser::eInstr Instr) {
    VMPushOneArgInstr(Instr);
    auto RetIndex = vm.GetEmptyRegIndex();
    exp_value_stack.push(RetIndex);
    VMPushRet(RetIndex);
}

void FooDriver::DealTwoArgExp(Parser::eInstr Instr) {
    VMPushTwoArgInstr(Instr);
    auto RetIndex = vm.GetEmptyRegIndex();
    exp_value_stack.push(RetIndex);
    VMPushRet(RetIndex);
}

void FooDriver::VMPushInstr(Parser::eInstr Instr) {
    vm.PushInstr(Instr);
}

void FooDriver::VMPushOneArgInstr(Parser::eInstr Instr) {
    vm.PushInstr(Instr);
    TransValueExp();
}

void FooDriver::VMPushTwoArgInstr(Parser::eInstr Instr) {
    vm.PushInstr(Instr);
    TransValueExp();
    TransValueExp();
}

void FooDriver::VMPushRet(Parser::tRegIndex Index) {
    vm.PushRet(Index);
}

void FooDriver::VMPushData(Parser::tIndex Index) {
    vm.PushData(Index);
}

// ��һ��ֵ�������沨�����ʽջ��ת�Ƶ��������
void FooDriver::TransValueExp() {
    auto Index = exp_value_stack.top();
    exp_value_stack.pop();
    vm.PushData(Index);
    if (vm.IsMemberIndex(Index)) {
        // �����ǰֵ����������ǰֵΪĳ������ĳ�Ա
        auto Count = vm.GetMemberLevel(Index);
        while (Count--) {
            // ���ݹ�������ѹ��ָ������
            Index = exp_value_stack.top();
            exp_value_stack.pop();
            vm.PushData(Index);
        }
    }
}

void FooDriver::Parse(const tCmdString& CmdString) {
    scanner.yyrestart();
    scanner.PushString(CmdString);
    int HasError = parser.parse();
    int Line = scanner.lineno();
    if (HasError) {
        fcs.LOG_ERROR() << "Line " << Line << " parse failed." << FooBase::Log::FooLogEnd;
        throw std::runtime_error("Parsed unsuccessfully.");
    }
    else{
        if (!vm.IsInMutual())
            vm.Execute();
        else
            ExitFooMode();
    }
}