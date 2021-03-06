#include <flex_reflect_plugin/EventHandler.hpp> // IWYU pragma: associated

#include <flexlib/ToolPlugin.hpp>
#include <flexlib/core/errors/errors.hpp>
#include <flexlib/utils.hpp>
#include <flexlib/funcParser.hpp>
#include <flexlib/inputThread.hpp>
#include <flexlib/clangUtils.hpp>
#include <flexlib/clangPipeline.hpp>
#include <flexlib/annotation_parser.hpp>
#include <flexlib/annotation_match_handler.hpp>
#include <flexlib/matchers/annotation_matcher.hpp>
#include <flexlib/options/ctp/options.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

namespace plugin {

namespace {

static const std::string kPluginDebugLogName = "(FlexReflect plugin)";

static const std::string kVersion = "v0.0.1";

static const std::string kVersionCommand = "/version";

#if !defined(APPLICATION_BUILD_TYPE)
#define APPLICATION_BUILD_TYPE "local build"
#endif

} // namespace

FlexReflectEventHandler::FlexReflectEventHandler()
{
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FlexReflectEventHandler::~FlexReflectEventHandler()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FlexReflectEventHandler::StringCommand(
  const ::plugin::ToolPlugin::Events::StringCommand& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::EventHandler::handle_event(StringCommand)");

  if(event.split_parts.size() == 1)
  {
    if(event.split_parts[0] == kVersionCommand) {
      LOG(INFO)
        << kPluginDebugLogName
        << " application version: "
        << kVersion;
      LOG(INFO)
        << kPluginDebugLogName
        << " application build type: "
        << APPLICATION_BUILD_TYPE;
    }
  }
}

/**
  * Provided annotation methods
  * - Call function (arbitrary logic) by some name
  * - Execute C++ code at runtime using Cling C++ interpreter
  **/
void FlexReflectEventHandler::RegisterAnnotationMethods(
  const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexReflect::handle_event(RegisterAnnotationMethods)");

#if defined(CLING_IS_ON)
  DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

  tooling_ = std::make_unique<ReflectTooling>(
    event
#if defined(CLING_IS_ON)
    , clingInterpreter_
#endif // CLING_IS_ON
  );

  DCHECK(event.annotationMethods);
  ::flexlib::AnnotationMethods& annotationMethods
    = *event.annotationMethods;

  // exports arbitrary C++ code, code can be multiline
  // unable to use `#include` or preprocessor macros
  /**
    EXAMPLE:
      // will be replaced with empty string
      __attribute__((annotate("{gen};{executeCode};\
      printf(\"Hello me!\");"))) \
      int SOME_UNIQUE_NAME1
      ;
      // if nothing printed, then
      // replace printf with
      // LOG(INFO)<<\"Hello!\";"))) \
  **/
  {
    VLOG(9)
      << "registered annotation method:"
         " executeCode";
    CHECK(tooling_);
    annotationMethods["{executeCode};"] =
      base::BindRepeating(
        &ReflectTooling::executeCode
        , base::Unretained(tooling_.get()));
  }

  // embeds arbitrary C++ code
  /**
    EXAMPLE:
      // will be replaced with 1234
      __attribute__((annotate("{gen};{executeCodeAndReplace};\
      new llvm::Optional<std::string>{\"1234\"};")))
      int SOME_UNIQUE_NAME2
      ;
  **/
  {
    VLOG(9)
      << "registered annotation method:"
         " executeCodeAndReplace";
    CHECK(tooling_);
    annotationMethods["{executeCodeAndReplace};"] =
      base::BindRepeating(
        &ReflectTooling::executeCodeAndReplace
        , base::Unretained(tooling_.get()));
  }

  /**
    EXAMPLE:
      #include <string>
      #include <vector>
      struct
        __attribute__((annotate("{gen};{funccall};make_reflect;")))
      SomeStructName {
       public:
        SomeStructName() {
          // ...
        }
       private:
        const int m_bar2 = 2;

        std::vector<std::string> m_VecStr2;
      };
      // handler for make_reflect must be registered by plugin!
  **/
  {
    VLOG(9)
      << "registered annotation method:"
         " funccall";
    CHECK(tooling_);
    annotationMethods["{funccall};"] =
      base::BindRepeating(
        &ReflectTooling::callFuncBySignature
        , base::Unretained(tooling_.get()));
  }
}

#if defined(CLING_IS_ON)
void FlexReflectEventHandler::RegisterClingInterpreter(
  const ::plugin::ToolPlugin::Events::RegisterClingInterpreter& event)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::EventHandler::handle_event(RegisterClingInterpreter)");

  DCHECK(event.clingInterpreter);
  clingInterpreter_ = event.clingInterpreter;
}
#endif // CLING_IS_ON

} // namespace plugin
