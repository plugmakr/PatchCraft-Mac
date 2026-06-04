#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>
#include <memory>
#include "LiveValueStore.h"

namespace patchcraft
{
    /**
        PScriptEngine parses and interprets the PatchCraft pScript language.
        It runs safe, sandboxed event-driven logic mapped to parameters and layers.
    */
    class PScriptEngine
    {
    public:
        PScriptEngine() = default;
        ~PScriptEngine() = default;

        // Compile script source code. Returns error message or empty string on success.
        juce::String compile (const juce::String& source);

        // Bind parameter store for live parameter evaluation.
        void bindStore (LiveValueStore* store) { valueStore = store; }

        // Execute specific event hooks.
        void triggerEvent (const juce::String& eventName, const std::map<juce::String, float>& eventArgs = {}, const juce::String& targetId = {});

        // Helper to check if script is compiled successfully.
        bool isCompiled() const { return compiled; }

        // Access source code.
        juce::String getSource() const { return sourceCode; }

    private:
        juce::String sourceCode;
        bool compiled = false;
        LiveValueStore* valueStore = nullptr;

        // --- Lexer & Parser Structures ---
        struct Expression
        {
            enum class Type { Constant, Identifier, Mapped, BinaryOp, UnaryMinus };
            Type type = Type::Constant;
            float value = 0.0f;
            juce::String identifier;

            // For mapped expression: source, srcMin, srcMax -> destMin, destMax
            juce::String mappedSource; // e.g. "velocity" or "modwheel"
            float srcMin = 0.0f, srcMax = 127.0f;
            float destMin = 0.0f, destMax = 1.0f;

            // For binary/unary operations
            char op = 0;
            std::shared_ptr<Expression> left;
            std::shared_ptr<Expression> right;
        };

        struct Condition
        {
            juce::String leftIdentifier; // e.g. "velocity"
            juce::String op;             // ">", "<", "==", ">=", "<="
            float rightValue = 0.0f;
        };

        struct Statement
        {
            enum class Kind { Set, PlayLayer, Randomize, Smooth, Repeat, TurnOnOff, IfCondition, Let, Print };
            Kind kind = Kind::Set;

            // set target to expression / let varName = expr
            juce::String target; // e.g. "filter.cutoff", "delay.feedback", "macro.Chaos"
            juce::String varName; // e.g. "x" in "let x = ..."
            Expression expr;

            // For Randomize
            float randMin = 0.0f, randMax = 1.0f;

            // For PlayLayer
            juce::String layerName;

            // For Smooth
            float smoothMs = 0.0f;

            // For TurnOnOff
            juce::String effectName;
            bool turnOn = true;

            // For conditions / nesting / loops
            Condition cond;
            std::vector<Statement> ifBody;
            std::vector<Statement> elseBody;
            std::vector<Statement> loopBody;
        };

        struct EventHandler
        {
            juce::String eventType; // "preset loads", "note starts", "note ends", "modwheel moves", "knob moves", etc.
            juce::String targetId; // e.g., for "when knob 'Cutoff' moves"
            std::vector<Statement> statements;
        };

        std::vector<EventHandler> handlers;

        // --- Compiler Helper Functions ---
        static std::vector<juce::String> tokenizeLine (const juce::String& line);
        static float parseValueWithUnit (const juce::String& text);
        
        static bool parseExpression (const juce::StringArray& tokens, int& index, Expression& expr);
        static bool parseSubExpression (const juce::StringArray& tokens, int& index, Expression& expr);
        static bool parseMulDivExpression (const juce::StringArray& tokens, int& index, Expression& expr);
        static bool parsePrimaryExpression (const juce::StringArray& tokens, int& index, Expression& expr);
        
        static bool parseCondition (const juce::StringArray& tokens, int& index, Condition& cond);
        
        struct LineInfo { juce::String content; int indent; int lineNum; };
        juce::String parseStatementBlock (const std::vector<LineInfo>& lines, int& lineIndex, int expectedIndent, std::vector<Statement>& body);
        static juce::String parseStatementInline (const juce::StringArray& tokens, int lineNum, Statement& stmt);
        
        // --- Interpreter Helper Functions ---
        void executeStatement (const Statement& stmt, const std::map<juce::String, float>& eventArgs);
        bool evaluateCondition (const Condition& cond, const std::map<juce::String, float>& eventArgs);
        float evaluateExpression (const Expression& expr, const std::map<juce::String, float>& eventArgs);
        
        // Maps user-friendly names to actual registry parameter IDs
        static juce::String resolveParameterId (const juce::String& friendlyName);

        std::map<juce::String, float> variables;

        class PScriptTimer : public juce::Timer
        {
        public:
            PScriptTimer (std::function<void()> callback, int intervalMs)
                : cb (callback)
            {
                startTimer (intervalMs);
            }
            ~PScriptTimer() override { stopTimer(); }

            void timerCallback() override { cb(); }

        private:
            std::function<void()> cb;
        };

        std::vector<std::unique_ptr<PScriptTimer>> activeTimers;
    };
}
