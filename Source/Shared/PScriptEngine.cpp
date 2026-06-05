#include "PScriptEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace patchcraft
{
    // A simple, helper parser for units
    float PScriptEngine::parseValueWithUnit (const juce::String& text)
    {
        auto trimmed = text.trim();
        if (trimmed.endsWith ("%"))
        {
            return trimmed.dropLastCharacters (1).getFloatValue() / 100.0f;
        }
        else if (trimmed.endsWithIgnoreCase ("Hz"))
        {
            return trimmed.dropLastCharacters (2).getFloatValue();
        }
        else if (trimmed.endsWithIgnoreCase ("dB"))
        {
            float db = trimmed.dropLastCharacters (2).getFloatValue();
            return juce::Decibels::decibelsToGain (db);
        }
        else if (trimmed.endsWithIgnoreCase ("st"))
        {
            return trimmed.dropLastCharacters (2).getFloatValue();
        }
        else if (trimmed.endsWithIgnoreCase ("ms"))
        {
            return trimmed.dropLastCharacters (2).getFloatValue();
        }
        return trimmed.getFloatValue();
    }

    // Tokenize a line while preserving string literals in quotes
    std::vector<juce::String> PScriptEngine::tokenizeLine (const juce::String& line)
    {
        std::vector<juce::String> tokens;
        juce::String current;
        bool inQuotes = false;

        for (int i = 0; i < line.length(); ++i)
        {
            auto c = line[i];

            if (c == '\"')
            {
                inQuotes = ! inQuotes;
                if (! inQuotes)
                {
                    tokens.push_back (current);
                    current.clear();
                }
            }
            else if (inQuotes)
            {
                current += c;
            }
            else
            {
                // Check for operators
                if (c == ':' || c == '=' || c == '>' || c == '<' || c == '+' || c == '*' || c == '/' || c == '(' || c == ')')
                {
                    if (current.isNotEmpty())
                    {
                        tokens.push_back (current);
                        current.clear();
                    }
                    tokens.push_back (juce::String::charToString (c));
                }
                else if (c == '-' && i + 1 < line.length() && line[i + 1] == '>')
                {
                    if (current.isNotEmpty())
                    {
                        tokens.push_back (current);
                        current.clear();
                    }
                    tokens.push_back ("->");
                    i++;
                }
                else if (c == '-')
                {
                    if (current.isNotEmpty())
                    {
                        tokens.push_back (current);
                        current.clear();
                    }
                    tokens.push_back ("-");
                }
                else if (c == '.' && i + 1 < line.length() && line[i + 1] == '.')
                {
                    if (current.isNotEmpty())
                    {
                        tokens.push_back (current);
                        current.clear();
                    }
                    tokens.push_back ("..");
                    i++;
                }
                else if (std::isspace (c))
                {
                    if (current.isNotEmpty())
                    {
                        tokens.push_back (current);
                        current.clear();
                    }
                }
                else
                {
                    current += c;
                }
            }
        }

        if (current.isNotEmpty())
            tokens.push_back (current);

        return tokens;
    }

    juce::String PScriptEngine::resolveParameterId (const juce::String& friendlyName)
    {
        auto normalized = friendlyName.trim().replace (".", "");
        
        if (normalized.equalsIgnoreCase ("filtercutoff") || normalized.equalsIgnoreCase ("cutoff"))
            return "filterCutoff";
        if (normalized.equalsIgnoreCase ("filterresonance") || normalized.equalsIgnoreCase ("resonance"))
            return "filterResonance";
        if (normalized.equalsIgnoreCase ("delayfeedback") || normalized.equalsIgnoreCase ("feedback"))
            return "delayFeedback";
        if (normalized.equalsIgnoreCase ("delaymix") || normalized.equalsIgnoreCase ("delay"))
            return "delayMix";
        if (normalized.equalsIgnoreCase ("delaytime") || normalized.equalsIgnoreCase ("time"))
            return "delayTime";
        if (normalized.equalsIgnoreCase ("reverbmix") || normalized.equalsIgnoreCase ("reverb"))
            return "reverbMix";
        
        // Dynamics
        if (normalized.equalsIgnoreCase ("dynthreshold") || normalized.equalsIgnoreCase ("threshold") || normalized.equalsIgnoreCase ("dynthresholddb"))
            return "dynThresholdDb";
        if (normalized.equalsIgnoreCase ("dynratio") || normalized.equalsIgnoreCase ("ratio"))
            return "dynRatio";
        if (normalized.equalsIgnoreCase ("dynattack") || normalized.equalsIgnoreCase ("dynattackms"))
            return "dynAttackMs";
        if (normalized.equalsIgnoreCase ("dynrelease") || normalized.equalsIgnoreCase ("dynreleasems"))
            return "dynReleaseMs";
        if (normalized.equalsIgnoreCase ("dynmakeup") || normalized.equalsIgnoreCase ("makeup") || normalized.equalsIgnoreCase ("dynmakeupdb"))
            return "dynMakeupDb";
        if (normalized.equalsIgnoreCase ("dynmix") || normalized.equalsIgnoreCase ("dynamics"))
            return "dynMix";
            
        // Chorus
        if (normalized.equalsIgnoreCase ("chorusrate"))
            return "chorusRate";
        if (normalized.equalsIgnoreCase ("chorusdepth"))
            return "chorusDepth";
        if (normalized.equalsIgnoreCase ("chorusfeedback"))
            return "chorusFeedback";
        if (normalized.equalsIgnoreCase ("chorusmix"))
            return "chorusMix";
            
        // Phaser
        if (normalized.equalsIgnoreCase ("phaserrate"))
            return "phaserRate";
        if (normalized.equalsIgnoreCase ("phaserdepth"))
            return "phaserDepth";
        if (normalized.equalsIgnoreCase ("phaserfeedback"))
            return "phaserFeedback";
        if (normalized.equalsIgnoreCase ("phasermix"))
            return "phaserMix";
            
        // Comb
        if (normalized.equalsIgnoreCase ("combfreq") || normalized.equalsIgnoreCase ("combfrequency"))
            return "combFreq";
        if (normalized.equalsIgnoreCase ("combfeedback"))
            return "combFeedback";
        if (normalized.equalsIgnoreCase ("combmix"))
            return "combMix";
            
        // Resonator
        if (normalized.equalsIgnoreCase ("resonatorfreq") || normalized.equalsIgnoreCase ("resonatorfrequency"))
            return "resonatorFreq";
        if (normalized.equalsIgnoreCase ("resonatorq"))
            return "resonatorQ";
        if (normalized.equalsIgnoreCase ("resonatormix"))
            return "resonatorMix";
            
        // Convolution
        if (normalized.equalsIgnoreCase ("convolutionsize") || normalized.equalsIgnoreCase ("convolutiontaps"))
            return "convolutionSize";
        if (normalized.equalsIgnoreCase ("convolutionmix"))
            return "convolutionMix";
            
        // Spectral
        if (normalized.equalsIgnoreCase ("spectraltilt"))
            return "spectralTilt";
        if (normalized.equalsIgnoreCase ("spectralmix"))
            return "spectralMix";
            
        // Tape
        if (normalized.equalsIgnoreCase ("tapedrive"))
            return "tapeDrive";
        if (normalized.equalsIgnoreCase ("tapetone"))
            return "tapeTone";
        if (normalized.equalsIgnoreCase ("tapeflutter"))
            return "tapeFlutter";
        if (normalized.equalsIgnoreCase ("tapemix"))
            return "tapeMix";
            
        // Vinyl
        if (normalized.equalsIgnoreCase ("vinylage"))
            return "vinylAge";
        if (normalized.equalsIgnoreCase ("vinyldust"))
            return "vinylDust";
        if (normalized.equalsIgnoreCase ("vinylwarp"))
            return "vinylWarp";
        if (normalized.equalsIgnoreCase ("vinylmix"))
            return "vinylMix";
            
        // Lo-Fi
        if (normalized.equalsIgnoreCase ("lofibits") || normalized.equalsIgnoreCase ("lofibit"))
            return "lofiBits";
        if (normalized.equalsIgnoreCase ("lofirate") || normalized.equalsIgnoreCase ("lofiratecrush"))
            return "lofiRate";
        if (normalized.equalsIgnoreCase ("lofimix"))
            return "lofiMix";
            
        // Vocal
        if (normalized.equalsIgnoreCase ("vocalformant"))
            return "vocalFormant";
        if (normalized.equalsIgnoreCase ("vocalbody"))
            return "vocalBody";
        if (normalized.equalsIgnoreCase ("vocalmix"))
            return "vocalMix";
            
        // MultiTap
        if (normalized.equalsIgnoreCase ("multitaptime"))
            return "multiTapTime";
        if (normalized.equalsIgnoreCase ("multitapfeedback"))
            return "multiTapFeedback";
        if (normalized.equalsIgnoreCase ("multitapspread"))
            return "multiTapSpread";
        if (normalized.equalsIgnoreCase ("multitapmix"))
            return "multiTapMix";
            
        // Output / Utility
        if (normalized.equalsIgnoreCase ("stereowidth") || normalized.equalsIgnoreCase ("width"))
            return "stereoWidth";
        if (normalized.equalsIgnoreCase ("monomaker"))
            return "monoMaker";
        if (normalized.equalsIgnoreCase ("volume") || normalized.equalsIgnoreCase ("vol"))
            return "volume";
        if (normalized.equalsIgnoreCase ("pan"))
            return "pan";
        if (normalized.equalsIgnoreCase ("projectbpm") || normalized.equalsIgnoreCase ("bpm"))
            return "projectBpm";
        if (normalized.equalsIgnoreCase ("bpmsync"))
            return "bpmSync";
        if (normalized.equalsIgnoreCase ("retrigger"))
            return "retrigger";
            
        return normalized;
    }

    bool PScriptEngine::parseExpression (const juce::StringArray& tokens, int& index, Expression& expr)
    {
        return parseSubExpression (tokens, index, expr);
    }

    bool PScriptEngine::parseSubExpression (const juce::StringArray& tokens, int& index, Expression& expr)
    {
        if (! parseMulDivExpression (tokens, index, expr))
            return false;

        while (index < tokens.size() && (tokens[index] == "+" || tokens[index] == "-"))
        {
            juce::String op = tokens[index];
            index++;

            Expression right;
            if (! parseMulDivExpression (tokens, index, right))
                return false;

            Expression leftCopy = expr;
            expr.type = Expression::Type::BinaryOp;
            expr.op = op[0];
            expr.left = std::make_shared<Expression> (leftCopy);
            expr.right = std::make_shared<Expression> (right);
        }
        return true;
    }

    bool PScriptEngine::parseMulDivExpression (const juce::StringArray& tokens, int& index, Expression& expr)
    {
        if (! parsePrimaryExpression (tokens, index, expr))
            return false;

        while (index < tokens.size() && (tokens[index] == "*" || tokens[index] == "/"))
        {
            juce::String op = tokens[index];
            index++;

            Expression right;
            if (! parsePrimaryExpression (tokens, index, right))
                return false;

            Expression leftCopy = expr;
            expr.type = Expression::Type::BinaryOp;
            expr.op = op[0];
            expr.left = std::make_shared<Expression> (leftCopy);
            expr.right = std::make_shared<Expression> (right);
        }
        return true;
    }

    bool PScriptEngine::parsePrimaryExpression (const juce::StringArray& tokens, int& index, Expression& expr)
    {
        if (index >= tokens.size())
            return false;

        if (tokens[index] == "-")
        {
            index++;
            Expression inner;
            if (! parsePrimaryExpression (tokens, index, inner))
                return false;

            expr.type = Expression::Type::UnaryMinus;
            expr.left = std::make_shared<Expression> (inner);
            return true;
        }

        if (tokens[index] == "(")
        {
            index++;
            if (! parseSubExpression (tokens, index, expr))
                return false;

            if (index >= tokens.size() || tokens[index] != ")")
                return false; // unmatched parentheses

            index++;
            return true;
        }

        juce::String firstToken = tokens[index];

        if (firstToken.equalsIgnoreCase ("velocity") || firstToken.equalsIgnoreCase ("modwheel"))
        {
            expr.type = Expression::Type::Mapped;
            expr.mappedSource = firstToken.toLowerCase();
            index++;

            if (index < tokens.size() && tokens[index].equalsIgnoreCase ("mapped"))
            {
                index++;
                
                if (index < tokens.size())
                {
                    expr.srcMin = parseValueWithUnit (tokens[index]);
                    index++;
                }
                if (index < tokens.size() && tokens[index] == "..")
                {
                    index++;
                }
                if (index < tokens.size())
                {
                    expr.srcMax = parseValueWithUnit (tokens[index]);
                    index++;
                }

                if (index < tokens.size() && tokens[index] == "->")
                {
                    index++;
                }

                juce::String destMinStr;
                if (index < tokens.size())
                {
                    destMinStr = tokens[index];
                    index++;
                    if (index < tokens.size() && (tokens[index].equalsIgnoreCase ("Hz") || tokens[index].equalsIgnoreCase ("dB") || tokens[index].equalsIgnoreCase ("st") || tokens[index].equalsIgnoreCase ("ms") || tokens[index] == "%"))
                    {
                        destMinStr += tokens[index];
                        index++;
                    }
                    expr.destMin = parseValueWithUnit (destMinStr);
                }

                if (index < tokens.size() && tokens[index] == "..")
                {
                    index++;
                }

                juce::String destMaxStr;
                if (index < tokens.size())
                {
                    destMaxStr = tokens[index];
                    index++;
                    if (index < tokens.size() && (tokens[index].equalsIgnoreCase ("Hz") || tokens[index].equalsIgnoreCase ("dB") || tokens[index].equalsIgnoreCase ("st") || tokens[index].equalsIgnoreCase ("ms") || tokens[index] == "%"))
                    {
                        destMaxStr += tokens[index];
                        index++;
                    }
                    expr.destMax = parseValueWithUnit (destMaxStr);
                }
                return true;
            }
            expr.type = Expression::Type::Identifier;
            expr.identifier = firstToken;
            return true;
        }

        juce::String valStr = firstToken;
        index++;
        if (index < tokens.size() && (tokens[index].equalsIgnoreCase ("Hz") || tokens[index].equalsIgnoreCase ("dB") || tokens[index].equalsIgnoreCase ("st") || tokens[index].equalsIgnoreCase ("ms") || tokens[index] == "%"))
        {
            valStr += tokens[index];
            index++;
        }

        bool isNumber = false;
        if (valStr.isNotEmpty())
        {
            auto firstChar = valStr[0];
            if (std::isdigit (firstChar) || firstChar == '.' || firstChar == '-' || firstChar == '+')
                isNumber = true;
        }
        if (isNumber || valStr.contains ("%") || valStr.containsIgnoreCase ("dB") || valStr.containsIgnoreCase ("Hz"))
        {
            expr.type = Expression::Type::Constant;
            expr.value = parseValueWithUnit (valStr);
        }
        else
        {
            expr.type = Expression::Type::Identifier;
            expr.identifier = valStr;
        }

        return true;
    }

    bool PScriptEngine::parseCondition (const juce::StringArray& tokens, int& index, Condition& cond)
    {
        if (index >= tokens.size())
            return false;

        cond.leftIdentifier = tokens[index];
        index++;

        if (index < tokens.size())
        {
            cond.op = tokens[index];
            index++;
            if (index < tokens.size() && tokens[index] == "=" && (cond.op == ">" || cond.op == "<" || cond.op == "="))
            {
                cond.op += "=";
                index++;
            }
        }

        if (index < tokens.size())
        {
            juce::String valStr = tokens[index];
            index++;
            if (index < tokens.size() && (tokens[index].equalsIgnoreCase ("Hz") || tokens[index].equalsIgnoreCase ("dB") || tokens[index].equalsIgnoreCase ("st") || tokens[index].equalsIgnoreCase ("ms") || tokens[index] == "%"))
            {
                valStr += tokens[index];
                index++;
            }
            cond.rightValue = parseValueWithUnit (valStr);
        }

        return true;
    }

    juce::String PScriptEngine::parseStatementInline (const juce::StringArray& tokens, int lineNum, Statement& stmt)
    {
        if (tokens.isEmpty())
            return {};

        if (tokens[0].equalsIgnoreCase ("let"))
        {
            stmt.kind = Statement::Kind::Let;
            if (tokens.size() < 4 || tokens[2] != "=")
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected syntax 'let <name> = <expression>'.";
            }
            stmt.varName = tokens[1];
            int expIdx = 3;
            if (! parseExpression (tokens, expIdx, stmt.expr))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Malformed expression.";
            }
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("set"))
        {
            stmt.kind = Statement::Kind::Set;
            if (tokens.size() < 4 || ! tokens[2].equalsIgnoreCase ("to"))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected syntax 'set <target> to <expression>'.";
            }
            stmt.target = resolveParameterId (tokens[1]);
            int expIdx = 3;
            if (! parseExpression (tokens, expIdx, stmt.expr))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Malformed expression.";
            }
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("print"))
        {
            stmt.kind = Statement::Kind::Print;
            int expIdx = 1;
            if (! parseExpression (tokens, expIdx, stmt.expr))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Malformed expression in print statement.";
            }
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("play"))
        {
            stmt.kind = Statement::Kind::PlayLayer;
            if (tokens.size() < 3 || ! tokens[1].equalsIgnoreCase ("layer"))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected syntax 'play layer \"LayerName\"'.";
            }
            stmt.layerName = tokens[2];
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("randomize"))
        {
            stmt.kind = Statement::Kind::Randomize;
            if (tokens.size() < 6 || ! tokens[2].equalsIgnoreCase ("between") || ! tokens[4].equalsIgnoreCase ("and"))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected syntax 'randomize <target> between <min> and <max>'.";
            }
            stmt.target = resolveParameterId (tokens[1]);
            stmt.randMin = parseValueWithUnit (tokens[3]);
            stmt.randMax = parseValueWithUnit (tokens[5]);
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("smooth"))
        {
            stmt.kind = Statement::Kind::Smooth;
            if (tokens.size() < 2)
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected smooth duration.";
            }
            stmt.smoothMs = parseValueWithUnit (tokens[1]);
            return {};
        }
        else if (tokens[0].equalsIgnoreCase ("turn"))
        {
            stmt.kind = Statement::Kind::TurnOnOff;
            if (tokens.size() < 4 || ! tokens[2].equalsIgnoreCase ("effect"))
            {
                return "Compile Error (Line " + juce::String (lineNum) + "): Expected syntax 'turn on/off effect \"EffectName\"'.";
            }
            stmt.turnOn = tokens[1].equalsIgnoreCase ("on");
            stmt.effectName = tokens[3];
            return {};
        }

        return "Compile Error (Line " + juce::String (lineNum) + "): Unknown statement keyword '" + tokens[0] + "'.";
    }

    juce::String PScriptEngine::parseStatementBlock (const std::vector<LineInfo>& lines, int& lineIndex, int expectedIndent, std::vector<Statement>& body)
    {
        while (lineIndex < (int) lines.size())
        {
            const auto& line = lines[(size_t) lineIndex];
            
            if (line.indent < expectedIndent)
                break;
                
            if (line.indent > expectedIndent)
            {
                return "Compile Error (Line " + juce::String (line.lineNum) + "): Unexpected indentation.";
            }
            
            auto rawTokens = tokenizeLine (line.content);
            juce::StringArray tokens;
            for (const auto& t : rawTokens)
                tokens.add (t);
                
            if (tokens.isEmpty())
            {
                lineIndex++;
                continue;
            }
            
            Statement stmt;
            
            if (tokens[0].equalsIgnoreCase ("let") ||
                tokens[0].equalsIgnoreCase ("set") ||
                tokens[0].equalsIgnoreCase ("print") ||
                tokens[0].equalsIgnoreCase ("play") ||
                tokens[0].equalsIgnoreCase ("randomize") ||
                tokens[0].equalsIgnoreCase ("smooth") ||
                tokens[0].equalsIgnoreCase ("turn"))
            {
                auto err = parseStatementInline (tokens, line.lineNum, stmt);
                if (err.isNotEmpty())
                    return err;
                body.push_back (stmt);
                lineIndex++;
            }
            else if (tokens[0].equalsIgnoreCase ("repeat"))
            {
                stmt.kind = Statement::Kind::Repeat;
                
                int colonIdx = -1;
                for (int i = 1; i < tokens.size(); ++i)
                {
                    if (tokens[i] == ":")
                    {
                        colonIdx = i;
                        break;
                    }
                }
                
                if (colonIdx == -1)
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected ':' in repeat statement.";
                }
                
                juce::StringArray countTokens;
                for (int i = 1; i < colonIdx; ++i)
                    countTokens.add (tokens[i]);
                    
                int expIdx = 0;
                if (countTokens.isEmpty() || ! parseExpression (countTokens, expIdx, stmt.expr))
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Malformed expression in repeat statement.";
                }
                
                if (colonIdx == tokens.size() - 1)
                {
                    // Block-based
                    lineIndex++;
                    
                    if (lineIndex >= (int) lines.size())
                    {
                        return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after repeat.";
                    }
                    
                    int childIndent = lines[(size_t) lineIndex].indent;
                    if (childIndent <= expectedIndent)
                    {
                        return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after repeat.";
                    }
                    
                    auto err = parseStatementBlock (lines, lineIndex, childIndent, stmt.loopBody);
                    if (err.isNotEmpty())
                        return err;
                }
                else
                {
                    // Inline
                    juce::StringArray subTokens;
                    for (int i = colonIdx + 1; i < tokens.size(); ++i)
                        subTokens.add (tokens[i]);
                        
                    Statement subStmt;
                    auto err = parseStatementInline (subTokens, line.lineNum, subStmt);
                    if (err.isNotEmpty())
                        return err;
                        
                    stmt.loopBody.push_back (subStmt);
                    lineIndex++;
                }
                
                body.push_back (stmt);
            }
            else if (tokens[0].equalsIgnoreCase ("if"))
            {
                stmt.kind = Statement::Kind::IfCondition;
                
                int colonIdx = -1;
                for (int i = 1; i < tokens.size(); ++i)
                {
                    if (tokens[i] == ":")
                    {
                        colonIdx = i;
                        break;
                    }
                }
                
                if (colonIdx == -1)
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected ':' in if statement.";
                }
                
                juce::StringArray condTokens;
                for (int i = 1; i < colonIdx; ++i)
                    condTokens.add (tokens[i]);
                    
                int condIdx = 0;
                if (condTokens.isEmpty() || ! parseCondition (condTokens, condIdx, stmt.cond))
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Malformed if condition.";
                }
                
                if (colonIdx == tokens.size() - 1)
                {
                    // Block-based body
                    lineIndex++;
                    
                    if (lineIndex >= (int) lines.size())
                    {
                        return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after if.";
                    }
                    
                    int childIndent = lines[(size_t) lineIndex].indent;
                    if (childIndent <= expectedIndent)
                    {
                        return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after if.";
                    }
                    
                    auto err = parseStatementBlock (lines, lineIndex, childIndent, stmt.ifBody);
                    if (err.isNotEmpty())
                        return err;
                }
                else
                {
                    // Inline body
                    juce::StringArray subTokens;
                    for (int i = colonIdx + 1; i < tokens.size(); ++i)
                        subTokens.add (tokens[i]);
                        
                    Statement subStmt;
                    auto err = parseStatementInline (subTokens, line.lineNum, subStmt);
                    if (err.isNotEmpty())
                        return err;
                        
                    stmt.ifBody.push_back (subStmt);
                    lineIndex++;
                }
                
                if (lineIndex < (int) lines.size())
                {
                    const auto& nextLine = lines[(size_t) lineIndex];
                    if (nextLine.indent == expectedIndent && nextLine.content.startsWithIgnoreCase ("else"))
                    {
                        auto elseTokens = tokenizeLine (nextLine.content);
                        int elseColonIdx = -1;
                        for (int i = 1; i < (int) elseTokens.size(); ++i)
                        {
                            if (elseTokens[(size_t) i] == ":")
                            {
                                elseColonIdx = i;
                                break;
                            }
                        }
                        
                        if (elseColonIdx == -1)
                        {
                            return "Compile Error (Line " + juce::String (nextLine.lineNum) + "): Expected ':' after else.";
                        }
                        
                        if (elseColonIdx == (int) elseTokens.size() - 1)
                        {
                            // Block-based else
                            lineIndex++;
                            
                            if (lineIndex >= (int) lines.size())
                            {
                                return "Compile Error (Line " + juce::String (nextLine.lineNum) + "): Expected indented block after else.";
                            }
                            
                            int elseChildIndent = lines[(size_t) lineIndex].indent;
                            if (elseChildIndent <= expectedIndent)
                            {
                                return "Compile Error (Line " + juce::String (nextLine.lineNum) + "): Expected indented block after else.";
                            }
                            
                            auto elseErr = parseStatementBlock (lines, lineIndex, elseChildIndent, stmt.elseBody);
                            if (elseErr.isNotEmpty())
                                return elseErr;
                        }
                        else
                        {
                            // Inline else
                            juce::StringArray subTokens;
                            for (int i = elseColonIdx + 1; i < (int) elseTokens.size(); ++i)
                                subTokens.add (elseTokens[(size_t) i]);
                                
                            Statement subStmt;
                            auto err = parseStatementInline (subTokens, nextLine.lineNum, subStmt);
                            if (err.isNotEmpty())
                                return err;
                                
                            stmt.elseBody.push_back (subStmt);
                            lineIndex++;
                        }
                    }
                }
                
                body.push_back (stmt);
            }
            else
            {
                return "Compile Error (Line " + juce::String (line.lineNum) + "): Unknown statement keyword '" + tokens[0] + "'.";
            }
        }
        return {};
    }

    juce::String PScriptEngine::compile (const juce::String& source)
    {
        handlers.clear();
        activeTimers.clear();
        variables.clear();

        {
            std::unique_lock<std::mutex> lock (telemetryMutex);
            pendingLogs.clear();
            pendingVariableUpdates.clear();
        }
        
        sourceCode = source;
        compiled = false;

        juce::StringArray rawLines;
        rawLines.addLines (source);

        std::vector<LineInfo> lines;
        for (int i = 0; i < rawLines.size(); ++i)
        {
            juce::String line = rawLines[i];
            int commentPos = line.indexOf ("#");
            if (commentPos >= 0)
                line = line.substring (0, commentPos);

            juce::String trimmed = line.trim();
            if (trimmed.isEmpty())
                continue;

            int indent = 0;
            for (int cIdx = 0; cIdx < line.length(); ++cIdx)
            {
                auto c = line[cIdx];
                if (c == ' ') indent++;
                else if (c == '\t') indent += 4;
                else break;
            }
            lines.push_back ({ trimmed, indent, i + 1 });
        }

        int lineIndex = 0;
        while (lineIndex < (int) lines.size())
        {
            const auto& line = lines[(size_t) lineIndex];
            if (line.indent != 0)
            {
                return "Compile Error (Line " + juce::String (line.lineNum) + "): Declaration must be at root level.";
            }

            if (line.content.startsWithIgnoreCase ("script "))
            {
                lineIndex++;
                continue;
            }

            if (line.content.startsWithIgnoreCase ("when "))
            {
                juce::String decl = line.content.substring (5).trim();
                if (! decl.endsWithChar (':'))
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected ':' at end of when declaration.";
                }
                decl = decl.dropLastCharacters (1).trim();

                EventHandler handler;
                
                if (decl.startsWithIgnoreCase ("knob "))
                {
                    handler.eventType = "knob moves";
                    int firstQuote = decl.indexOfChar ('\"');
                    int secondQuote = decl.indexOfChar (firstQuote + 1, '\"');
                    if (firstQuote >= 0 && secondQuote > firstQuote)
                        handler.targetId = decl.substring (firstQuote + 1, secondQuote);
                    else
                        handler.targetId = decl.substring (5).fromLastOccurrenceOf ("moves", false, true).trim();
                }
                else if (decl.startsWithIgnoreCase ("pad "))
                {
                    int firstQuote = decl.indexOfChar ('\"');
                    int secondQuote = decl.indexOfChar (firstQuote + 1, '\"');
                    if (firstQuote >= 0 && secondQuote > firstQuote)
                        handler.targetId = decl.substring (firstQuote + 1, secondQuote);
                    
                    if (decl.containsIgnoreCase ("held"))
                        handler.eventType = "pad held";
                    else
                        handler.eventType = "pad released";
                }
                else if (decl.startsWithIgnoreCase ("timer "))
                {
                    handler.eventType = decl.toLowerCase();
                }
                else
                {
                    handler.eventType = decl.toLowerCase();
                }

                lineIndex++;

                if (lineIndex >= (int) lines.size())
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after when declaration.";
                }

                int expectedIndent = lines[(size_t) lineIndex].indent;
                if (expectedIndent == 0)
                {
                    return "Compile Error (Line " + juce::String (line.lineNum) + "): Expected indented block after when declaration.";
                }

                auto err = parseStatementBlock (lines, lineIndex, expectedIndent, handler.statements);
                if (err.isNotEmpty())
                    return err;

                handlers.push_back (handler);
            }
            else
            {
                return "Compile Error (Line " + juce::String (line.lineNum) + "): Unknown block type. Declare an event using 'when ... :'.";
            }
        }

        compiled = true;

        for (const auto& handler : handlers)
        {
            if (handler.eventType.startsWith ("timer "))
            {
                int intervalMs = 100;
                auto valStr = handler.eventType.substring (6).trim();
                int num = valStr.getIntValue();
                if (num > 0)
                {
                    intervalMs = num;
                    if (valStr.containsIgnoreCase ("s") && !valStr.containsIgnoreCase ("ms"))
                        intervalMs *= 1000;
                }
                intervalMs = juce::jlimit (10, 10000, intervalMs);
                
                juce::String eventType = handler.eventType;
                activeTimers.push_back (std::make_unique<PScriptTimer> ([this, eventType] {
                    juce::MessageManager::callAsync ([this, eventType] {
                        triggerEvent (eventType, {});
                    });
                }, intervalMs));
            }
        }

        return {};
    }

    void PScriptEngine::triggerEvent (const juce::String& eventName, const std::map<juce::String, float>& eventArgs, const juce::String& targetId)
    {
        if (! compiled)
            return;

        const auto name = eventName.toLowerCase().trim();
        for (const auto& handler : handlers)
        {
            if (handler.eventType == name)
            {
                if (handler.targetId.isNotEmpty() && targetId.isNotEmpty())
                {
                    auto resolvedHandler = resolveParameterId (handler.targetId).toLowerCase();
                    auto resolvedTarget  = resolveParameterId (targetId).toLowerCase();
                    if (resolvedHandler != resolvedTarget)
                        continue;
                }

                for (const auto& stmt : handler.statements)
                {
                    executeStatement (stmt, eventArgs);
                }
            }
        }
    }

    void PScriptEngine::executeStatement (const Statement& stmt, const std::map<juce::String, float>& eventArgs)
    {
        if (stmt.kind == Statement::Kind::IfCondition)
        {
            if (evaluateCondition (stmt.cond, eventArgs))
            {
                for (const auto& sub : stmt.ifBody)
                    executeStatement (sub, eventArgs);
            }
            else
            {
                for (const auto& sub : stmt.elseBody)
                    executeStatement (sub, eventArgs);
            }
            return;
        }

        if (stmt.kind == Statement::Kind::Let)
        {
            float val = evaluateExpression (stmt.expr, eventArgs);
            variables[stmt.varName] = val;
            
            std::unique_lock<std::mutex> lock (telemetryMutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                pendingVariableUpdates.push_back (VariableUpdate { stmt.varName, val });
            }
            return;
        }

        if (stmt.kind == Statement::Kind::Print)
        {
            juce::String logMsg;
            if (stmt.expr.type == Expression::Type::Identifier)
            {
                auto name = stmt.expr.identifier;
                auto it = eventArgs.find (name.toLowerCase());
                if (it != eventArgs.end())
                {
                    logMsg = name + " = " + juce::String (it->second);
                    DBG ("[pScript Print] " + logMsg);
                }
                else
                {
                    auto varIt = variables.find (name);
                    if (varIt != variables.end())
                    {
                        logMsg = name + " = " + juce::String (varIt->second);
                        DBG ("[pScript Print] " + logMsg);
                    }
                    else if (valueStore != nullptr && valueStore->getRaw (name) != nullptr)
                    {
                        logMsg = name + " = " + juce::String (valueStore->getValue (name));
                        DBG ("[pScript Print] " + logMsg);
                    }
                    else
                    {
                        logMsg = name;
                        DBG ("[pScript Print] " + logMsg);
                    }
                }
            }
            else
            {
                float val = evaluateExpression (stmt.expr, eventArgs);
                logMsg = juce::String (val);
                DBG ("[pScript Print] " + logMsg);
            }
            
            std::unique_lock<std::mutex> lock (telemetryMutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                pendingLogs.push_back (LogMessage { logMsg, juce::Time::getCurrentTime() });
            }
            return;
        }

        if (stmt.kind == Statement::Kind::Repeat)
        {
            float countVal = evaluateExpression (stmt.expr, eventArgs);
            int count = juce::jlimit (0, 1000, (int) countVal);
            for (int i = 0; i < count; ++i)
            {
                for (const auto& sub : stmt.loopBody)
                    executeStatement (sub, eventArgs);
            }
            return;
        }

        if (valueStore == nullptr)
            return;

        if (stmt.kind == Statement::Kind::Set)
        {
            float val = evaluateExpression (stmt.expr, eventArgs);
            valueStore->setValue (stmt.target, val);
        }
        else if (stmt.kind == Statement::Kind::Randomize)
        {
            float minVal = stmt.randMin;
            float maxVal = stmt.randMax;
            float r = juce::Random::getSystemRandom().nextFloat();
            float val = minVal + r * (maxVal - minVal);
            valueStore->setValue (stmt.target, val);
        }
        else if (stmt.kind == Statement::Kind::PlayLayer)
        {
            valueStore->setValue ("layer_" + stmt.layerName + "_gate", 1.0f);
        }
        else if (stmt.kind == Statement::Kind::TurnOnOff)
        {
            valueStore->setValue ("effect_" + stmt.effectName + "_bypass", stmt.turnOn ? 0.0f : 1.0f);
        }
    }

    bool PScriptEngine::evaluateCondition (const Condition& cond, const std::map<juce::String, float>& eventArgs)
    {
        float left = 0.0f;
        auto it = eventArgs.find (cond.leftIdentifier.toLowerCase());
        if (it != eventArgs.end())
            left = it->second;
        else
        {
            auto varIt = variables.find (cond.leftIdentifier);
            if (varIt != variables.end())
                left = varIt->second;
            else if (valueStore != nullptr)
                left = valueStore->getValue (cond.leftIdentifier);
        }

        float right = cond.rightValue;
        if (cond.op == ">") return left > right;
        if (cond.op == "<") return left < right;
        if (cond.op == "==") return std::abs (left - right) < 0.0001f;
        if (cond.op == ">=") return left >= right;
        if (cond.op == "<=") return left <= right;
        return false;
    }

    float PScriptEngine::evaluateExpression (const Expression& expr, const std::map<juce::String, float>& eventArgs)
    {
        if (expr.type == Expression::Type::Constant)
        {
            return expr.value;
        }
        else if (expr.type == Expression::Type::Identifier)
        {
            auto name = expr.identifier;
            auto it = eventArgs.find (name.toLowerCase());
            if (it != eventArgs.end())
                return it->second;
            
            auto varIt = variables.find (name);
            if (varIt != variables.end())
                return varIt->second;
            
            if (valueStore != nullptr)
                return valueStore->getValue (name);
            return 0.0f;
        }
        else if (expr.type == Expression::Type::Mapped)
        {
            float sourceVal = 0.0f;
            auto it = eventArgs.find (expr.mappedSource);
            if (it != eventArgs.end())
                sourceVal = it->second;
            else
            {
                auto varIt = variables.find (expr.mappedSource);
                if (varIt != variables.end())
                    sourceVal = varIt->second;
                else if (valueStore != nullptr)
                    sourceVal = valueStore->getValue (expr.mappedSource);
            }

            float t = (sourceVal - expr.srcMin) / (expr.srcMax - expr.srcMin);
            t = juce::jlimit (0.0f, 1.0f, t);
            return expr.destMin + t * (expr.destMax - expr.destMin);
        }
        else if (expr.type == Expression::Type::UnaryMinus)
        {
            return -evaluateExpression (*expr.left, eventArgs);
        }
        else if (expr.type == Expression::Type::BinaryOp)
        {
            float leftVal = evaluateExpression (*expr.left, eventArgs);
            float rightVal = evaluateExpression (*expr.right, eventArgs);

            switch (expr.op)
            {
                case '+': return leftVal + rightVal;
                case '-': return leftVal - rightVal;
                case '*': return leftVal * rightVal;
                case '/': return (rightVal != 0.0f) ? (leftVal / rightVal) : 0.0f;
                default: break;
            }
        }
        return 0.0f;
    }

    std::vector<LogMessage> PScriptEngine::getPendingLogs()
    {
        std::unique_lock<std::mutex> lock (telemetryMutex);
        std::vector<LogMessage> logsCopy;
        logsCopy.swap (pendingLogs);
        return logsCopy;
    }

    std::vector<VariableUpdate> PScriptEngine::getPendingVariableUpdates()
    {
        std::unique_lock<std::mutex> lock (telemetryMutex);
        std::vector<VariableUpdate> updatesCopy;
        updatesCopy.swap (pendingVariableUpdates);
        return updatesCopy;
    }
}
