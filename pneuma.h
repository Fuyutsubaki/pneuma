#pragma once
#include<string>
#include<vector>
#include<memory>
#include<algorithm>
#include<functional>
#include<array>
#include<regex>
#include<boost\scope_exit.hpp>

struct Pneuma_DebugData
{
	const Pneuma_DebugData& operator+=(const Pneuma_DebugData&rhs)
	{
		return *this;
	}

};
template<class T,class ITERATOR,class RESULT_TYPE,class BUILD_TYPE> //Buildで返す型
class basic_pneuma{
public:
	typedef ITERATOR Iterator;
	typedef RESULT_TYPE result_type;
	typedef BUILD_TYPE build_type;
	struct BNF_error:std::runtime_error
	{
		BNF_error(const std::string&e)
			:runtime_error(e)
		{}
	};
	struct Ast_error:std::runtime_error
	{
		Ast_error(const std::string&e)
			:runtime_error(e)
		{}
	};
protected:
	class Ast;
	typedef std::function<result_type()> Call_Func;
	typedef std::shared_ptr<Call_Func> CallFuncPtr;
	typedef std::shared_ptr<Ast> ast_ptr;
	typedef std::weak_ptr<Ast> ast_weak;
	class Ast :public std::enable_shared_from_this<Ast>
	{
	public:
		friend basic_pneuma;
		Ast(const CallFuncPtr&func)
			:func(func)
		{}
		virtual const T&GetElement(size_t n)const=0;
		virtual size_t element_size()const=0;
		virtual size_t child_size()const=0;
		virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const=0;
		
	private:
		result_type eval(ast_weak &now_ast_ptr)const
		{
			auto temp=now_ast_ptr;
			now_ast_ptr=shared_from_this(); 
			BOOST_SCOPE_EXIT((&now_ast_ptr)(&temp)){
				now_ast_ptr=temp;
			}BOOST_SCOPE_EXIT_END
			if(func)
				return (*func)();
			else
			{
				if(child_size()==0)
					throw BNF_error("DefaultFuncError");
				for(size_t i=0;i<child_size()-1;++i)
					Child(i,now_ast_ptr);
				return Child(child_size()-1,now_ast_ptr);
			}
		};
		CallFuncPtr func;
	};

	ast_weak _now_astptr;
	result_type Child(size_t n)
	{
		if(auto i=_now_astptr.lock())
			return i->Child(n,_now_astptr);
		throw Ast_error("BadChildCall");
	}
	size_t ChildSize()const
	{
		if(auto i=_now_astptr.lock())
			return i->child_size();
		throw Ast_error("BadChildSizeCall");
	}
	const T&GetToken(size_t n)
	{
		if(auto i=_now_astptr.lock())
			return i->GetElement(n);
		throw Ast_error("BadGetTokenCall");
	}
	size_t TokenSize()const
	{
		if(auto i=_now_astptr.lock())
			return i->element_size();
		throw Ast_error("CantTokenSizeGet");
	}

private:
	template<class Data,class Asts>
	static ast_ptr MakeAst(Data&data,Asts&ast,const CallFuncPtr&func)
	{
		class normall_Ast :public Ast
		{
		public:
			normall_Ast(Data&data,Asts&ast,const CallFuncPtr&func)
				:child_data(ast.begin(),ast.end())
				,element_data(data.begin(),data.end())
				,Ast(func)
			{}
			virtual const T&GetElement(size_t n)const{return element_data.at(n);}
			virtual size_t element_size()const{return element_data.size();}
			virtual size_t child_size()const{return child_data.size();}
			virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const
			{
				return child_data.at(n)->eval(now_ast_ptr);
			}
		
		private:
			std::vector<T> element_data;
			std::vector<ast_ptr> child_data;
		};
		return std::make_shared<normall_Ast>(data,ast,func);
	}
	

	static ast_ptr MakeAst(const CallFuncPtr&func)
	{
		class func_Ast :public Ast
		{
		public:
			func_Ast(const CallFuncPtr&func)
				:Ast(func)
			{}
			virtual const T&GetElement(size_t n)const{throw Ast_error("BadAccsessElement");}
			virtual size_t element_size()const{return 0;}
			virtual size_t child_size()const{return 0;}
			virtual const result_type Child(size_t n,ast_weak &now_ast_ptr)const{throw Ast_error("BadAccsessElement");	}
		};
		return std::make_shared<func_Ast>(func);
	}
protected:

	//holder(=構文木)が必要とするデータ
	struct holder_data
	{
		holder_data()
			:func(CallFuncPtr(nullptr))
		{}
		std::vector<T> st;
		std::vector<ast_ptr> ast;
		CallFuncPtr func;
		const holder_data&operator+=(const holder_data&rhs)
		{
			if(rhs.func)
				func=rhs.func;
			st.insert(st.end(),rhs.st.begin(),rhs.st.end());
			ast.insert(ast.end(),rhs.ast.begin(),rhs.ast.end());
			return *this;
		}
	};


public:
	typedef Pneuma_DebugData DebugData;
protected:
	class Factor
	{
	public:
		virtual bool operator()(Iterator &it,const Iterator&end,holder_data&,DebugData&)const=0;
	};
	typedef std::shared_ptr<Factor> Factor_Ptr;
	class Symbol;
	

	class Facter_expr
	{
	public:
		Facter_expr(const Factor_Ptr&pt)
		{
			data.push_back(pt);
		}
		Facter_expr(std::vector<Factor_Ptr>&&original)
			:data(std::move(original))
		{}
		
		Facter_expr operator&(const Facter_expr&rhs)const
		{
			std::vector<Factor_Ptr> temp;
			temp.reserve(data.size()+rhs.data.size());
			temp=data;
			temp.insert(temp.end(),rhs.data.begin(),rhs.data.end());
			return std::move(temp);
		}
		inline Facter_expr operator |(const Facter_expr&rhs)const
		{
			class Or:public Factor
			{
			public:
				Or(const Facter_expr&lhs,const Facter_expr&rhs)
					:lhs(lhs),rhs(rhs)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					holder_data h;
					DebugData r;
					Iterator i=it;
					if((*lhs)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
						return true;
					}
					return (*rhs)(it,end,holder,result);
				}
			private:
				Factor_Ptr lhs,rhs;
			};
			return Factor_Ptr (new Or(*this,rhs));  
		}
		inline Facter_expr operator *()const
		{
			class Loop:public Factor
			{
			public:
				Loop(const Facter_expr&rhs)
					:rhs(rhs)
				{}

				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					holder_data h;
					DebugData r;
					Iterator i=it;
					if((*rhs)(i,end,h,r) && (*this)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
					}
					return true;
				}
			private:
				Factor_Ptr rhs;
			};
			return Factor_Ptr (new Loop(*this));  
		}
		inline Facter_expr operator +()const
		{
			return *this&*(*this);
		}
		inline Facter_expr operator!()const
		{
			class Option:public Factor
			{
			public:
				Option(const Facter_expr&rhs)
					:rhs(rhs)
				{}

				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					holder_data h;
					DebugData r;
					Iterator i=it;
					if((*rhs)(i,end,h,r))
					{
						holder+=h;
						result+=r;
						it=i;
					}
					return true;
				}
			private:
				Factor_Ptr rhs;
			};
			return Factor_Ptr (new Option(*this));  

		}
		operator Factor_Ptr()const
		{
			class wrapper_Facter_expr:public Factor
			{
			public:
				wrapper_Facter_expr(const std::vector<Factor_Ptr>&data)
					:data(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
				{
					return 
						std::all_of(
						data.begin(),data.end(),[&](const Factor_Ptr&factor){
							return (*factor)(it,end,holder,result);
					});
				}
			private:
				std::vector<Factor_Ptr> data;
			};
			return Factor_Ptr (new wrapper_Facter_expr(data)); 
		}
		Factor_Ptr Wrap()const
		{
			return *this;
		}
		const std::vector<Factor_Ptr>&Get()const
		{
			return data;
		}
	private:
		std::vector<Factor_Ptr> data;
	};
	typedef std::vector<Factor_Ptr> HolderData;
	class Symbol
	{
	public:
		Symbol(HolderData&data)
			:data(data)
		{}
		inline void operator=(const Facter_expr&rhs)
		{
			data=rhs.Get();
		}
		inline void operator=(const Symbol&rhs)
		{
			data=static_cast<Facter_expr>(rhs).Get();
		}
		bool operator()(Iterator &it,const Iterator&end,holder_data& hold,DebugData&result)const
		{
			holder_data myhold;
			if(std::all_of(data.begin(),data.end(),[&](const Factor_Ptr&factor){return (*factor)(it,end,myhold,result);}))
			{
				if(myhold.st.size()==0 && !myhold.func && myhold.ast.size()==1)
				{
					hold.ast.push_back(myhold.ast.at(0));
				}
				else
				{
					hold.ast.push_back(MakeAst(myhold.st,myhold.ast,myhold.func));
				}
				return true;
			}
			return false;
		}
		operator Facter_expr()const
		{
			class ref_Holder:public Factor
			{
			public:
				ref_Holder(HolderData &data)
					:holder(data)
				{}
				bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
				{
					return holder(it,end,hold,result);
				}
			private:
				Symbol holder;
			};
			return Factor_Ptr (new ref_Holder(data));
		}
		inline Facter_expr operator&(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)&rhs;
		}
		inline Facter_expr operator|(const Facter_expr&rhs)const
		{
			return static_cast<Facter_expr>(*this)|rhs;
		}
		inline Facter_expr operator*()const
		{
			return *static_cast<Facter_expr>(*this);
		}
		inline Facter_expr operator!()const
		{
			return !static_cast<Facter_expr>(*this);
		}
		inline Facter_expr operator+()const
		{
			return +static_cast<Facter_expr>(*this);
		}
		inline HolderData&GetHolderData()
		{
			return data;
		}
	private:
		HolderData &data;
	};

	Symbol MakeSymbol()
	{
		auto p=std::make_shared<HolderData>();
		node_data.push_back(p);
		return Symbol(*p);
	}
	void SetStart(const Facter_expr&rhs)
	{
		start=std::make_shared<HolderData>();
		Symbol h(*start);
		h=rhs;
	}
		
private:
	typedef std::shared_ptr<HolderData> HolderDataPtr;
	std::vector<HolderDataPtr> node_data;
	HolderDataPtr start;
	holder_data holder;
public:
	bool Parse(Iterator&it,const Iterator&end,DebugData&debug)
	{
		if(!start)throw BNF_error("NotDefinedStart");
		Symbol node(*start);
		return node(it,end,holder,debug);
	}
	virtual build_type Build()=0;
protected:
	result_type eval()
	{
		return (*holder.ast.at(0)).eval(_now_astptr);
	}

	static Facter_expr Set(const Call_Func&func)
	{
		class SetCallFunc:public Factor
		{
		public:
			SetCallFunc(const Call_Func&f)
				:fun(std::make_shared<Call_Func>(f))
			{}
			bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
			{
				holder.func=fun;
				return true;
			}
		private:
			CallFuncPtr fun;
		};
		return Factor_Ptr(new SetCallFunc(func));
	}
	//普通の終端記号用のテンプレート。funcは条件、pushはトークンを保存するか
	template<class FUNC,bool PUSH>
	class basic_terminal_symbol:public Factor
	{
	public:
		basic_terminal_symbol(const FUNC&f)
			:f(f)
		{}
		bool operator()(Iterator &it,const Iterator&end,holder_data&holder,DebugData&result)const
		{
			if(it!=end && f(*it))
			{
				if(PUSH)
					holder.st.push_back(*it);
				++it;
				return true;
			}
			return false;
		}
	private:
		FUNC f;
	}; 
private:
	class _Match
	{
	public:
		_Match(const T&x)
			:data(x)
		{}
		bool operator()(const T&x)const
		{
			return x==data;
		}
	private:
		T data;
	};
protected:
	static Facter_expr S(const T&code)
	{
		
		typedef basic_terminal_symbol<_Match,false> sym;
		return Factor_Ptr(new sym(_Match(code)));
	}
	static Facter_expr SP(const T&code)
	{
		typedef basic_terminal_symbol<_Match,true> sym;
		return Factor_Ptr(new sym(_Match(code)));
	}

	//pneuma→symbol
	
	template<class PNEUMA>
	struct PneumaWrapperData
	{
		typedef std::function<std::shared_ptr<PNEUMA>()> Data;
		PneumaWrapperData(const Data&data):data(data)
		{}
		std::function<std::shared_ptr<PNEUMA>()> data;
		PneumaWrapperData()
			:data([]{return std::make_shared<PNEUMA>();})
		{}
		//VS2013来たら本気出す
		template<class U1>
		PneumaWrapperData<PNEUMA> MakePneumaWrapperData(U1&&a1)
			:([]{return std::make_shared<PNEUMA>(a1);})
		{}
		
		template<class U1,class U2>
		PneumaWrapperData<PNEUMA> MakePneumaWrapperData(U1&&a1,U2&&a2)
			:([]{return std::make_shared<PNEUMA>(a1,a2);})
		{}

		template<class U1,class U2,class U3>
		PneumaWrapperData<PNEUMA> MakePneumaWrapperData(U1&&a1,U2&&a2,U3&&a3)
			:([]{return std::make_shared<PNEUMA>(a1,a2);})
		{}

		template<class U1,class U2,class U3,class U4>
		PneumaWrapperData<PNEUMA> MakePneumaWrapperData(U1&&a1,U2&&a2,U3&&a3,U4&&a4)
			:([]{return std::make_shared<PNEUMA>(a1,a2,a3,a4);})
		{}
	};
	template<class PneumaData>
	Facter_expr wrapper(const PneumaData&data)
	{
		class pneuma2symbol:public Factor
		{
		public:
			pneuma2symbol(const PneumaData&f)
				:data(f)
			{}
			bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
			{
				auto p=data.data();
				if(p->Parse(it,end,result))
				{
					Call_Func f=[=]{return p->Build();};
					hold.ast.push_back(MakeAst(std::make_shared<Call_Func>(f)));
					return true;
				}
				return false;
			}
		private:
			PneumaData data;
		}; 
		return Factor_Ptr(new pneuma2symbol(data.data));
	}

	template<class PneumaData,class TransFunction>
	Facter_expr wrapper(const PneumaData&data,const TransFunction &func)
	{
		class pneuma2symbol:public Factor
		{
		public:
			pneuma2symbol(const Maker&f,const TransFunction &trans)
				:data(f),trans(trans)
			{}
			bool operator()(Iterator &it,const Iterator&end,holder_data&hold,DebugData&result)const
			{
				auto p=data.data();
				if(p->Parse(it,end,result))
				{
					Call_Func f=[=]{return trans(p->Build());};
					hold.ast.push_back(MakeAst(std::make_shared<Call_Func>(f)));
					return true;
				}
				return false;
			}
		private:
			PneumaData data;
			TransFunction trans;
		}; 
		return Factor_Ptr(new pneuma2symbol(data.data));
	}
};

template<class Iterator,class result_type,class BuildType>
class pneuma:public basic_pneuma<std::string,Iterator,result_type,BuildType>
{
private:
	class _Reg
	{
	public:
		_Reg(const std::string&regex)
			:re(regex)
		{}
		bool operator()(const std::string&x)const
		{
			return std::regex_match(x,re);
		}
	private:
		std::regex re;
	};
protected:
	static Facter_expr RegexSymbol(const std::string&code)
	{
		typedef basic_terminal_symbol<_Reg,true> sym;
		return Factor_Ptr(new sym(_Reg(code)));
	}
	static Facter_expr RegexPushSymbol(const std::string&code)
	{
		typedef basic_terminal_symbol<_Reg,true> sym;
		return Factor_Ptr(new sym(_Reg(code)));
	}
};