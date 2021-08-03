#include "framework.hpp"
#include "processor.hpp"

neuro::Processor *neuro::Processor::make(const string &name, json &params)
{
  string es;

  if (name != "caspian") {
    es = (string) "Processor::make() called with a name ("
       + name
       + (string) ") not equal to caspian";
    throw std::runtime_error(es);
  }
  return new caspian::Processor(params);
}

