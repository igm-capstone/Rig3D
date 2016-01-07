#pragma once

namespace Rig3D
{
	template <class Base>
	class Singleton : public Base
	{
	public:
		static Singleton& SharedInstance()
		{
			static Singleton sharedInstance;
			return sharedInstance;
		}

	private:
		Singleton() {};
		~Singleton() {};

		Singleton(Singleton const&) = delete;
		void operator=(Singleton const&) = delete;
	};

	template <template <class> class Derived, class Base>
	class TSingleton : public Derived<Base>
	{
	public:
		static TSingleton& SharedInstance()
		{
			static TSingleton sharedInstance;
			return sharedInstance;
		}

	private:
		TSingleton() {};
		~TSingleton() {};

		TSingleton(TSingleton const&) = delete;
		void operator=(TSingleton const&) = delete;
	};
}