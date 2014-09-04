//
// Main interface for clients to the MWT service.
//

#include "stdafx.h"
#include <typeinfo>

class Base_Function_Wrapper { };
class MWT_Empty { };

template <class T>
class Stateful_Function_Wrapper : public Base_Function_Wrapper
{
public:
	typedef Action Policy_Func(T* state_Context, Context& application_Context, Action_Set& actions);

	Policy_Func* Policy_Function;
};

class Stateless_Function_Wrapper : public Base_Function_Wrapper
{
public:
	typedef Action Policy_Func(Context& application_Context, Action_Set& actions);

	Policy_Func* Policy_Function;
};

// TODO: for exploration budget, exploration algo should implement smth like Start & Stop Explore, Adjust epsilon
class Explorer : public Policy
{
};

template <class T>
class Epsilon_Greedy_Explorer : public Explorer
{
public:
	Epsilon_Greedy_Explorer(
		float epsilon, 
		Base_Function_Wrapper& default_Policy_Func_Wrapper, 
		T* default_Policy_Func_State_Context) :
			epsilon(epsilon), 
			default_Policy_Wrapper(default_Policy_Func_Wrapper),
			pDefault_Policy_State_Context(default_Policy_Func_State_Context)
	{
		if (epsilon <= 0)
		{
			throw std::invalid_argument("Initial epsilon value must be positive.");
		}
		random_Generator = new PRG<u32>();
	}

	~Epsilon_Greedy_Explorer()
	{
		delete random_Generator;
	}

	std::pair<Action, float> Choose_Action(Context& context, Action_Set& actions)
	{
		// Invoke the default policy function to get the action
		Action* chosen_Action = nullptr;
		if (typeid(default_Policy_Wrapper) == typeid(Stateless_Function_Wrapper))
		{
			Stateless_Function_Wrapper* stateless_Function_Wrapper = (Stateless_Function_Wrapper*)(&default_Policy_Wrapper);
			chosen_Action = &stateless_Function_Wrapper->Policy_Function(context, actions);
		}
		else
		{
			Stateful_Function_Wrapper<T>* stateful_Function_Wrapper = (Stateful_Function_Wrapper<T>*)(&default_Policy_Wrapper);
			chosen_Action = &stateful_Function_Wrapper->Policy_Function(pDefault_Policy_State_Context, context, actions);
		}

		float action_Probability = 0.f;
		float base_Probability = epsilon / actions.Count(); // uniform probability
		
		// TODO: check this random generation
		if (((float)random_Generator->uniform_Int() / (2e32 - 1)) < 1.f - epsilon)
		{
			action_Probability = 1.f - epsilon + base_Probability;
		}
		else
		{
			// Get uniform random action ID
			u32 actionId = (uint32_t)ceil(random_Generator->uniform_Int(0, actions.Count() - 1));

			if (actionId == chosen_Action->Get_Id())
			{
				// IF it matches the one chosen by the default policy
				// then increase the probability
				action_Probability = 1.f - epsilon + base_Probability;
			}
			else
			{
				// Otherwise it's just the uniform probability
				action_Probability = base_Probability;
			}
			chosen_Action = &actions.Get(actionId);
		}

		return std::pair<Action, float>(*chosen_Action, action_Probability);
	}

private:
	float epsilon;
	PRG<u32>* random_Generator;

	Base_Function_Wrapper& default_Policy_Wrapper;
	T* pDefault_Policy_State_Context;
};

class MWT
{
public:
	MWT(std::string& appId)
	{
		Id_Generator::Initialize();

		if (appId.empty())
		{
			appId = this->Generate_App_Id();
		}

		pLogger = new Logger(appId);
	}

	~MWT()
	{
		delete pLogger;
		delete pExplorer;
	}

	// TODO: should we restrict explorationBudget to some small numbers to prevent users from unwanted effect?
	template <class T>
	void Initialize_Epsilon_Greedy(
		float epsilon, 
		typename Stateful_Function_Wrapper<T>::Policy_Func defaultPolicyFunc, 
		T* defaultPolicyFuncStateContext)
	{
		Stateful_Function_Wrapper<T>* func_Wrapper = new Stateful_Function_Wrapper<T>();
		func_Wrapper->Policy_Function = &defaultPolicyFunc;
		
		pExplorer = new Epsilon_Greedy_Explorer<T>(epsilon, *func_Wrapper, defaultPolicyFuncStateContext);
		
		pDefault_Func_Wrapper = func_Wrapper;
	}

	void Initialize_Epsilon_Greedy(
		float epsilon, 
		Stateless_Function_Wrapper::Policy_Func default_Policy_Func)
	{
		Stateless_Function_Wrapper* func_Wrapper = new Stateless_Function_Wrapper();
		func_Wrapper->Policy_Function = default_Policy_Func;
		
		pExplorer = new Epsilon_Greedy_Explorer<MWT_Empty>(epsilon, *func_Wrapper, nullptr);
		
		pDefault_Func_Wrapper = func_Wrapper;
	}

	// TODO: should include defaultPolicy here? From users view, it's much more intuitive
	std::pair<Action, u64> Choose_Action_Join_Key(Context& context, Action_Set& actions)
	{
		std::pair<Action, float> action_Probability_Pair = pExplorer->Choose_Action(context, actions);
		Interaction pInteraction(&context, action_Probability_Pair.first, action_Probability_Pair.second);
		pLogger->Store(&pInteraction);
		
		// TODO: Anything else to do here?

		return std::pair<Action, u64>(action_Probability_Pair.first, pInteraction.Get_Id());
	}

	Action Choose_Action(Context& context, Action_Set& actions)
	{
		// TODO: Anything else to do here?

		return pExplorer->Choose_Action(context, actions).first;
	}

private:
	// TODO: App ID + Interaction ID is the unique identifier
	// Users can specify a seed and we use it to generate app id for them
	// so we can guarantee uniqueness.
	std::string Generate_App_Id()
	{
		return ""; // TODO: implement
	}

private:
	std::string appId;
	Explorer* pExplorer;
	Logger* pLogger;
	Base_Function_Wrapper* pDefault_Func_Wrapper;
};