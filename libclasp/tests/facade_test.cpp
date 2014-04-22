// 
// Copyright (c) 2006, Benjamin Kaufmann
// 
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/ 
// 
// Clasp is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// Clasp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Clasp; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
#include "test.h"
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/clasp_facade.h>
#include <clasp/minimize_constraint.h>
#include <clasp/heuristics.h>

namespace Clasp { namespace Test {
using namespace Clasp::mt;

class FacadeTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(FacadeTest);
	CPPUNIT_TEST(testIncrementalSolve);
	CPPUNIT_TEST(testIncrementalEnum);
	CPPUNIT_TEST(testIncrementalCons);
	CPPUNIT_TEST(testIncrementalMin);
	CPPUNIT_TEST(testUpdateConfig);
	CPPUNIT_TEST(testIncrementalProjectUpdate);
	CPPUNIT_TEST(testStats);
#if WITH_THREADS
	CPPUNIT_TEST(testAsyncSolveTrivialUnsat);
	CPPUNIT_TEST(testInterruptBeforeSolve);
	CPPUNIT_TEST(testCancelAsyncOperation);
	CPPUNIT_TEST(testAsyncResultDtorCancelsOp);
	CPPUNIT_TEST(testDestroyAsyncResultNoFacade);
	CPPUNIT_TEST(testDestroyDanglingAsyncResult);
	CPPUNIT_TEST(testCancelDanglingAsyncOperation);
#endif
	CPPUNIT_TEST_SUITE_END(); 
public:
	typedef ClaspFacade::Result Result;
	void testRunIncremental(Result::Base stop, int maxStep, int minStep, Result::Base expectedRes, int expectedSteps) {
		ClaspConfig config;
		ClaspFacade f;
		f.ctx.enableStats(2);
		config.asp.noEq();
		Asp::LogicProgram& asp = f.startAsp(config, true);
		Result::Base       res = Result::UNKNOWN;
		do {
			f.update();
			switch(f.step()) {
			default: break;
			case 0:
				asp.setAtomName(1, "a").setAtomName(2, "b").setAtomName(3, "x")
					 .startRule().addHead(1).addToBody(2, true).endRule()
					 .startRule().addHead(2).addToBody(1, true).endRule()
					 .startRule().addHead(1).addToBody(3, true).endRule()
					 .freeze(3)
					 .setCompute(1, true);
				break;
			case 1:
				asp.setAtomName(4, "y").setAtomName(5, "z").endRule()
				   .startRule().addHead(3).addToBody(4, true).endRule()
				   .startRule().addHead(4).addToBody(3, true).endRule()
				   .startRule().addHead(4).addToBody(5, true).endRule()
				   .freeze(5);
				break;
			case 2:
				asp.setAtomName(6, "q").setAtomName(7, "r")
				   .startRule().addHead(5).addToBody(6, true).addToBody(7, true).endRule()
				   .startRule().addHead(6).addToBody(3, false).endRule()
				   .startRule().addHead(7).addToBody(1, false).addToBody(2, false).endRule()
				   .startRule(Asp::CHOICERULE).addHead(5).endRule();
				break;
			case 3:
				asp.setAtomName(8,"f").startRule(Asp::CHOICERULE).addHead(8).endRule();
				break;
			}
			if (!f.prepare()) { break; }
			res = f.solve();
		} while (--maxStep && ((minStep > 0 && --minStep) || res != stop));
		CPPUNIT_ASSERT_EQUAL(expectedRes,  (Result::Base)f.result());
		CPPUNIT_ASSERT_EQUAL(expectedSteps, f.step());
	}
	void testIncrementalSolve() {
		testRunIncremental(Result::SAT  , -1, -1, Result::SAT  , 2);
		testRunIncremental(Result::UNSAT, -1, -1, Result::UNSAT, 0);
		testRunIncremental(Result::SAT  ,  2, -1, Result::UNSAT, 1);
		testRunIncremental(Result::SAT  , -1,  4, Result::SAT  , 3);
	}
	void testIncrementalEnum() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		config.enumerate.numModels = 0;
		config.enumerate.type = EnumOptions::enum_record;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(Clasp::ClaspFacade::enum_volatile);
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum == 2);
		CPPUNIT_ASSERT(libclasp.update().ok());
		asp.setAtomName(2, "b");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(2).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum == 4);
	}
	void testIncrementalCons() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		config.enumerate.numModels = 0;
		config.enumerate.type = EnumOptions::enum_cautious;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a").setAtomName(2, "b").setAtomName(3, "c");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).addHead(2).addHead(3).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		config.enumerate.type = EnumOptions::enum_brave;
		libclasp.update(true);
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum > 1);
	}
	void testIncrementalMin() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		config.enumerate.numModels = 0;
		config.enumerate.type = EnumOptions::enum_auto;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a").setAtomName(2, "b").setAtomName(3, "c");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).addHead(2).addHead(3).endRule();
		asp.startRule(Clasp::Asp::OPTIMIZERULE).addToBody(1,true).addToBody(2, true).addToBody(3, true).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum < 8u);
		libclasp.update().disposeMinimizeConstraint();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum == 8);
	}
	void testUpdateConfig() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		config.enumerate.numModels = 0;
		config.enumerate.type = EnumOptions::enum_auto;
		config.addSolver(0).heuId  = Heuristic_t::heu_berkmin;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a").setAtomName(2, "b").setAtomName(3, "c");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).addHead(2).addHead(3).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		config.addSolver(0).heuId = Heuristic_t::heu_vsids;
		libclasp.update(true);
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(dynamic_cast<ClaspVsids*>(libclasp.ctx.master()->heuristic().get()));
	}
	void testIncrementalProjectUpdate() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		config.enumerate.numModels = 0;
		config.enumerate.type      = EnumOptions::enum_auto;
		config.enumerate.project   = 1;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "_a").setAtomName(2, "b");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).addHead(2).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.ctx.varInfo(libclasp.ctx.symbolTable()[2].lit.var()).project());
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum == 2);
		config.enumerate.project = 0;
		libclasp.update(true);
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT(libclasp.summary().numEnum == 4);
		config.enumerate.project = 1;
		libclasp.update(true);
		asp.setAtomName(3, "_x").setAtomName(4, "y");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(3).addHead(4).endRule();
		libclasp.prepare();
		CPPUNIT_ASSERT(libclasp.ctx.varInfo(libclasp.ctx.symbolTable()[2].lit.var()).project());
		CPPUNIT_ASSERT(libclasp.ctx.varInfo(libclasp.ctx.symbolTable()[4].lit.var()).project());
		CPPUNIT_ASSERT(libclasp.solve().sat());
		CPPUNIT_ASSERT_EQUAL(uint64(4), libclasp.summary().numEnum);
	}
	void testStats() {
		Clasp::ClaspFacade libclasp;
		Clasp::ClaspConfig config;
		Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a").setAtomName(2, "b").setAtomName(3, "c");
		asp.startRule(Clasp::Asp::CHOICERULE).addHead(1).addHead(2).addHead(3).endRule();
		asp.startRule(Clasp::Asp::OPTIMIZERULE).addToBody(1, true).addToBody(2, true).endRule();
		libclasp.prepare();
		libclasp.solve();
		std::vector<std::string> keys;
		getKeys(libclasp, keys, "");
		CPPUNIT_ASSERT(!keys.empty());
		for (std::vector<std::string>::const_iterator it = keys.begin(), end = keys.end(); it != end; ++it) {
			CPPUNIT_ASSERT(libclasp.getStat(it->c_str()).valid());
		}
		CPPUNIT_ASSERT(libclasp.getStat("lp.rules") == double(asp.stats.rules()));
		CPPUNIT_ASSERT(libclasp.getStat("numEnum") == double(libclasp.summary().enumerated()));
		CPPUNIT_ASSERT(libclasp.getStat("solvers.choices") == double(libclasp.ctx.master()->stats.choices));
		CPPUNIT_ASSERT(libclasp.getStat("costs.0") == double(libclasp.summary().costs()->optimum(0)));
		CPPUNIT_ASSERT(libclasp.getStat("optimal") == double(libclasp.summary().optimal()));
	}
	void getKeys(const ClaspFacade& f, std::vector<std::string>& out, const std::string& p) {
		typedef std::pair<std::string, double> Stat;
		std::size_t len = 0;
		for (const char* k = f.getKeys(p.c_str()); (len = std::strlen(k)) != 0; k += len + 1) {
			if (k[len-1] != '.') { 
				out.push_back(p+k);
				if (std::strcmp(k, "__len") == 0 && f.getStat(out.back().c_str()) > 0.0) {
					if (f.getKeys((p+"0").c_str())) { getKeys(f, out, p+"0."); }
					else                            { out.push_back(p+"0"); }
				}
			}
			else                 { getKeys(f, out, p+k); }
		}
	}
#if WITH_THREADS
	typedef ClaspFacade::AsyncResult AsyncResult;
	void testAsyncSolveTrivialUnsat() {
		ClaspConfig config;
		ClaspFacade libclasp;
		config.enumerate.numModels = 0;
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule().addHead(1).addToBody(1, false).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		AsyncResult it = libclasp.startSolveAsync();
		CPPUNIT_ASSERT(it.end());
		CPPUNIT_ASSERT(it.get().unsat());
	}
	void testInterruptBeforeSolve() {
		ClaspConfig config;
		ClaspFacade libclasp;
		config.enumerate.numModels = 0;
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		libclasp.terminate(2);
		AsyncResult it = libclasp.startSolveAsync();
		CPPUNIT_ASSERT(it.end());
		CPPUNIT_ASSERT(it.get().interrupted());
	}
	void testCancelAsyncOperation() {
		ClaspConfig config;
		ClaspFacade libclasp;
		config.enumerate.numModels = 0;
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		AsyncResult it = libclasp.startSolveAsync();
		while (!it.end()) { 
			CPPUNIT_ASSERT(it.model().num == 1);
			if (it.cancel()){ break; }
		}
		CPPUNIT_ASSERT(it.end() && !libclasp.solving() && it.interrupted());
		libclasp.update();
		libclasp.prepare(ClaspFacade::enum_volatile);
		it = libclasp.startSolveAsync();
		int mod = 0;
		while (!it.end()) { ++mod; it.next(); }
		CPPUNIT_ASSERT(!libclasp.solving() && mod == 2);
	}
	void testAsyncResultDtorCancelsOp() {
		ClaspConfig config;
		ClaspFacade libclasp;
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		{ AsyncResult it = libclasp.startSolveAsync(); }
		CPPUNIT_ASSERT(!libclasp.solving() && libclasp.result().interrupted());
	}
	void testDestroyAsyncResultNoFacade() {
		ClaspConfig  config;
		AsyncResult* handle   = 0;
		ClaspFacade* libclasp = new ClaspFacade();
		Asp::LogicProgram& asp= libclasp->startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp->prepare(ClaspFacade::enum_volatile);
		handle = new AsyncResult( libclasp->startSolveAsync() );
		delete libclasp;
		CPPUNIT_ASSERT(handle->interrupted());
		CPPUNIT_ASSERT_NO_THROW(delete handle);
	}
	void testDestroyDanglingAsyncResult() {
		ClaspConfig  config;
		ClaspFacade  libclasp;
		AsyncResult* handle = 0;
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		handle = new AsyncResult( libclasp.solveAsync() );
		handle->wait();
		libclasp.update();
		libclasp.prepare(ClaspFacade::enum_volatile);
		AsyncResult* it = new AsyncResult(libclasp.startSolveAsync());
		delete handle;
		CPPUNIT_ASSERT(!it->interrupted() && libclasp.solving());
		CPPUNIT_ASSERT_NO_THROW(delete it);
		CPPUNIT_ASSERT(!libclasp.solving());
	}
	void testCancelDanglingAsyncOperation() {
		ClaspConfig  config;
		ClaspFacade  libclasp;
		
		Asp::LogicProgram& asp = libclasp.startAsp(config, true);
		asp.setAtomName(1, "a");
		asp.startRule(Asp::CHOICERULE).addHead(1).endRule();
		libclasp.prepare(ClaspFacade::enum_volatile);
		AsyncResult step0 = libclasp.solveAsync();
		step0.wait();
		libclasp.update();
		libclasp.prepare(ClaspFacade::enum_volatile);
		AsyncResult step1 = libclasp.startSolveAsync();
		
		CPPUNIT_ASSERT_EQUAL(false, step0.cancel());
		CPPUNIT_ASSERT(libclasp.solving());
		CPPUNIT_ASSERT_EQUAL(true, step1.cancel());
		CPPUNIT_ASSERT(!libclasp.solving());
	}
#endif
};
CPPUNIT_TEST_SUITE_REGISTRATION(FacadeTest);
} } 

